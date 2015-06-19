
/*
 * To whom it may concern: http://womble.decadent.org.uk/readdir_r-advisory.html
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#ifdef HAVE_FNMATCH_H
#include <fnmatch.h>
#endif

#include "db.h"
#include "report.h"
#include "duc.h"
#include "private.h"
#include "uthash.h"
#include "utlist.h"
#include "buffer.h"

#ifndef HAVE_LSTAT
#define lstat stat
#endif

struct hard_link {
	struct duc_devino devino;
	UT_hash_handle hh;
};

struct exclude {
	char *name;
	struct exclude *next;
};

struct duc_index_req {
	duc *duc;
	struct exclude *exclude_list;
	dev_t dev;
	duc_index_flags flags;
	int maxdepth;
	duc_index_progress_cb progress_fn;
	void *progress_fndata;
	int progress_n;
	struct timeval progress_interval;
	struct timeval progress_time;
	struct hard_link *hard_link_map;
};

struct scanner {
	struct scanner *parent;
	int depth;
	char *path;
	int fd;
	DIR *d;
	char db_key[32];
	time_t mtime;
	size_t db_key_len;
	struct buffer *buffer;
	struct buffer *buffer_prev;
	struct duc_devino devino;
	struct duc_size size;
	struct duc *duc;
	struct duc_index_req *req;
	struct duc_index_report *rep;
};


static void index_dir(struct scanner *scanner_dir);



static void st_to_size(struct stat *st, struct duc_size *s1)
{
	s1->apparent = st->st_size;
	s1->actual = st->st_blocks * 512;
}


static void st_to_devino(struct stat *st, struct duc_devino *devino)
{
	devino->dev = st->st_dev;
	devino->ino = st->st_ino;
}


static void size_accum(struct duc_size *s1, struct duc_size *s2)
{
	s1->actual += s2->actual;
	s1->apparent += s2->apparent;
}


duc_index_req *duc_index_req_new(duc *duc)
{
	struct duc_index_req *req = duc_malloc(sizeof(struct duc_index_req));
	memset(req, 0, sizeof *req);

	req->duc = duc;
	req->progress_interval.tv_sec = 0;
	req->progress_interval.tv_usec = 100 * 1000;
	req->hard_link_map = NULL;

	return req;
}


static void add_ent(struct scanner *scanner, const char *name, struct duc_size *size, uint8_t type, struct duc_devino *devino)
{
	struct buffer *b = scanner->buffer;

	buffer_put_string(b, name);
	buffer_put_varint(b, size->apparent);
	buffer_put_varint(b, size->actual);
	buffer_put_varint(b, type);

	if(type == DUC_FILE_TYPE_DIR) {
		buffer_put_varint(b, devino->dev);
		buffer_put_varint(b, devino->ino);
	}
}


int duc_index_req_free(duc_index_req *req)
{
	struct hard_link *h, *hn;
	struct exclude *e, *en;

	HASH_ITER(hh, req->hard_link_map, h, hn) {
		HASH_DEL(req->hard_link_map, h);
		free(h);
	}

	LL_FOREACH_SAFE(req->exclude_list, e, en) {
		free(e->name);
		free(e);
	}

	free(req);

	return 0;
}


int duc_index_req_add_exclude(duc_index_req *req, const char *patt)
{
	struct exclude *e = duc_malloc(sizeof(struct exclude));
	e->name = duc_strdup(patt);
	LL_APPEND(req->exclude_list, e);
	return 0;
}


int duc_index_req_set_maxdepth(duc_index_req *req, int maxdepth)
{
	req->maxdepth = maxdepth;
	return 0;
}


int duc_index_req_set_progress_cb(duc_index_req *req, duc_index_progress_cb fn, void *ptr)
{
	req->progress_fn = fn;
	req->progress_fndata = ptr;
	return DUC_OK;
}


static int match_exclude(const char *name, struct exclude *list)
{
	struct exclude *e;
	LL_FOREACH(list, e) {
#ifdef HAVE_FNMATCH_H
		if(fnmatch(e->name, name, 0) == 0) return 1;
#else
		if(strstr(name, e->name) == 0) return 1;
#endif
	}
	return 0;
}


/*
 * Convert st_mode to DUC_FILE_TYPE_* type
 */

duc_file_type st_to_type(mode_t mode)
{
	duc_file_type type = DUC_FILE_TYPE_UNKNOWN;

	if(S_ISBLK(mode))  type = DUC_FILE_TYPE_BLK;
	if(S_ISCHR(mode))  type = DUC_FILE_TYPE_CHR;
	if(S_ISDIR(mode))  type = DUC_FILE_TYPE_DIR;
	if(S_ISFIFO(mode)) type = DUC_FILE_TYPE_FIFO;
#ifndef S_ISLNK
	if(S_ISLNK(mode))  type = DUC_FILE_TYPE_LNK;
#endif
	if(S_ISREG(mode )) type = DUC_FILE_TYPE_REG;
#ifndef S_ISSOCK
	if(S_ISSOCK(mode)) type = DUC_FILE_TYPE_SOCK;
#endif

	return type;
}


/*
 * Check if the given node is a duplicate. returns 1 if this dev/inode
 * was seen before
 */

static int is_duplicate(struct duc_index_req *req, struct duc_devino *devino)
{
	struct hard_link *h;
	HASH_FIND(hh, req->hard_link_map, devino, sizeof(*devino), h);
	if(h) return 1;

	h = duc_malloc(sizeof *h);
	h->devino = *devino;
	HASH_ADD(hh, req->hard_link_map, devino, sizeof(h->devino), h);
	return 0;
}

	
/* 
 * Open dir and read file status 
 */

static struct scanner *scanner_new(struct duc *duc, struct scanner *scanner_parent, const char *path, time_t mtime, struct duc_size *size, struct duc_devino *devino)
{
	struct scanner *scanner;
	scanner = duc_malloc0(sizeof *scanner);

	scanner->duc = duc;
	scanner->buffer_prev = NULL;
	scanner->parent = scanner_parent;
	scanner->path = duc_strdup(path);

	int fd_parent = scanner_parent ? scanner_parent->fd : 0;

	scanner->fd = openat(fd_parent, path, O_RDONLY | O_NOCTTY | O_DIRECTORY | O_NOFOLLOW);
	if(scanner->fd == -1) {
		duc_log(duc, DUC_LOG_WRN, "Skipping %s: %s", path, strerror(errno));
		goto err;
	}


	/* If a stat for this directory was already done it is passed in *st
	 * and we use this. If not, we do a fstat here */

	if(mtime && devino && size) {
		scanner->mtime = mtime;
		scanner->devino = *devino;
		scanner->size = *size;
	} else {
		struct stat st;
		int r = fstat(scanner->fd, &st);
		if(r == -1) {
			duc_log(duc, DUC_LOG_WRN, "Error statting %s: %s", path, strerror(errno));
			goto err;
		}
		scanner->mtime = st.st_mtime;
		st_to_devino(&st, &scanner->devino);
		st_to_size(&st, &scanner->size);
	}


	/* Generate database key for this dir */

	scanner->db_key_len = snprintf(scanner->db_key, sizeof(scanner->db_key), 
			"%jx/%jx", (uintmax_t)scanner->devino.dev, (uintmax_t)scanner->devino.ino);


	/* Check if this directory is already in the database and if the mtime
	 * is unchanged. If so, we do not have to opendir() / readdir() /
	 * closedir(), but we can iterate the directory from the info from the
	 * database */

	size_t vall;
	void *val = db_get(duc->db, scanner->db_key, scanner->db_key_len, &vall);
	if(val) {
		uint64_t v;
		struct buffer *b = buffer_new(val, vall);
		buffer_get_varint(b, &v); time_t mtime = v;
		buffer_get_varint(b, &v); /* discard parent dev */
		buffer_get_varint(b, &v); /* discard parent ino */

		if(mtime == scanner->mtime) {
			scanner->buffer_prev = b;
		} else {
			buffer_free(b);

		}
	}

	if(scanner->buffer_prev == NULL) {
		scanner->d = fdopendir(scanner->fd);
		if(scanner->d == NULL) {
			duc_log(duc, DUC_LOG_WRN, "Skipping %s: %s", path, strerror(errno));
			goto err;
		}
	}


	/* Write dir header to database */

	scanner->buffer = buffer_new(NULL, 0);
	buffer_put_varint(scanner->buffer, scanner->mtime);

	if(scanner_parent) {
		scanner->depth = scanner_parent->depth + 1;
		scanner->duc = scanner_parent->duc;
		scanner->req = scanner_parent->req;
		scanner->rep = scanner_parent->rep;
		buffer_put_varint(scanner->buffer, scanner_parent->devino.dev);
		buffer_put_varint(scanner->buffer, scanner_parent->devino.ino);
	} else {
		buffer_put_varint(scanner->buffer, 0);
		buffer_put_varint(scanner->buffer, 0);
	}
		
	duc_log(duc, DUC_LOG_DMP, ">> %s", scanner->path);

	return scanner;

err:
	if(scanner->fd > 0) close(scanner->fd);
	if(scanner) free(scanner);
	return NULL;
}


static void scanner_free(struct scanner *scanner)
{
	struct duc *duc = scanner->duc;
	struct duc_index_req *req = scanner->req;
	struct duc_index_report *report = scanner->rep; 	
	
	duc_log(duc, DUC_LOG_DMP, "<< %s actual:%jd apparent:%jd", 
			scanner->path, scanner->size.apparent, scanner->size.actual);

	if(scanner->parent) {
		size_accum(&scanner->parent->size, &scanner->size);

		if(req->maxdepth == 0 || scanner->depth < req->maxdepth) 
			add_ent(scanner->parent, scanner->path, &scanner->size, DUC_FILE_TYPE_DIR, &scanner->devino);

	}
	
	/* Progress reporting */

	if(req->progress_fn) {

		if(!scanner->parent || req->progress_n++ == 100) {
			
			struct timeval t_now;
			gettimeofday(&t_now, NULL);

			if(!scanner->parent || timercmp(&t_now, &req->progress_time, > )) {
				req->progress_fn(report, req->progress_fndata);
				timeradd(&t_now, &req->progress_interval, &req->progress_time);
			}
			req->progress_n = 0;
		}

	}

	int r = db_put(duc->db, scanner->db_key, scanner->db_key_len, scanner->buffer->data, scanner->buffer->len);
	if(r != 0) {
		duc->err = r;
	}

	buffer_free(scanner->buffer);
	if(scanner->buffer_prev) {
		buffer_free(scanner->buffer_prev);
	}
	if(scanner->d) {
		closedir(scanner->d);
	} else {
		close(scanner->fd);
	}
	duc_free(scanner->path);
	duc_free(scanner);
}


static void index_dirent(struct scanner *scanner_dir, const char *name, duc_file_type type, time_t mtime, struct duc_size *size, struct duc_devino *devino)
{
	struct duc *duc = scanner_dir->duc;
	struct duc_index_req *req = scanner_dir->req;
	struct duc_index_report *report = scanner_dir->rep; 	

	/* Calculate size of this dirent */

	if(type == DUC_FILE_TYPE_DIR) {

		/* Open and scan child directory */

		struct scanner *scanner_ent = scanner_new(duc, scanner_dir, name, mtime, size, devino);

		if(scanner_ent) {
			index_dir(scanner_ent);
			scanner_free(scanner_ent);
		}

	} else {

		size_accum(&scanner_dir->size, size);
		size_accum(&report->size, size);

		report->file_count ++;

		duc_log(duc, DUC_LOG_DMP, "  %c %jd %jd %s", 
				duc_file_type_char(type), size->apparent, size->actual, name);


		/* Optionally hide file names */

		if(req->flags & DUC_INDEX_HIDE_FILE_NAMES) name = "<FILE>";


		/* Store record */

		if(req->maxdepth == 0 || scanner_dir->depth < req->maxdepth) 
			add_ent(scanner_dir, name, size, type, devino);
	}


}


/*
 * Index a directory from the filesystem
 */

static void index_dir_filesystem(struct scanner *scanner)
{
	struct duc *duc = scanner->duc;
	struct duc_index_req *req = scanner->req;
	//struct duc_index_report *report = scanner->rep; 	

	struct dirent *e;

	while( (e = readdir(scanner->d)) != NULL) {

		/* Skip . and .. */

		char *name = e->d_name;

		if(name[0] == '.') {
			if(name[1] == '\0') continue;
			if(name[1] == '.' && name[2] == '\0') continue;
		}

		if(match_exclude(name, req->exclude_list)) {
			duc_log(duc, DUC_LOG_WRN, "Skipping %s: excluded by user", name);
			continue;
		}

		/* Get file info. Derive the file type from st.st_mode. It
		 * seems that we can not trust e->d_type because it is not
		 * guaranteed to contain a sane value on all file system types.
		 * See the readdir() man page for more details */

		struct stat st_ent;
		int r = fstatat(scanner->fd, name, &st_ent, AT_SYMLINK_NOFOLLOW);
		if(r == -1) {
			duc_log(duc, DUC_LOG_WRN, "Error statting %s: %s", name, strerror(errno));
			continue;
		}

		duc_file_type type = st_to_type(st_ent.st_mode);
		struct duc_devino devino = { .dev = st_ent.st_dev, .ino = st_ent.st_ino };


		/* Skip hard link duplicates for any files with more then one hard link */

		if(type != DUC_FILE_TYPE_DIR && req->flags & DUC_INDEX_CHECK_HARD_LINKS && 
				st_ent.st_nlink > 1 && is_duplicate(req, &devino)) {
			continue;
		}


		/* Check if we can cross file system boundaries */

		if(type == DUC_FILE_TYPE_DIR && req->flags & DUC_INDEX_XDEV && st_ent.st_dev != req->dev) {
			duc_log(duc, DUC_LOG_WRN, "Skipping %s: not crossing file system boundaries", name);
			continue;
		}

		struct duc_size size_ent;
		struct duc_devino devino_ent;

		st_to_size(&st_ent, &size_ent);
		st_to_devino(&st_ent, &devino_ent);

		index_dirent(scanner, name, type, st_ent.st_mtime, &size_ent, &devino_ent);

	}
}	


/*
 * Index a directory from an earlier indexed directory we found in the database
 * with a matching mtime
 */

static void index_dir_cached(struct scanner *scanner)
{
	uint64_t v;

	struct buffer *b = scanner->buffer_prev;
	char *name;
	duc_file_type type;
	struct duc_size size;
	struct duc_devino devino;

	while(b->ptr < b->len) {

		buffer_get_string(b, &name);
		buffer_get_varint(b, &v); size.apparent = v;
		buffer_get_varint(b, &v); size.actual = v;
		buffer_get_varint(b, &v); type = v;

		if(type == DUC_FILE_TYPE_DIR) {
			buffer_get_varint(b, &v); devino.dev = v;
			buffer_get_varint(b, &v); devino.ino = v;
		}

		index_dirent(scanner, name, type, 0, &size, &devino);
	}
}


/*
 * Index a directory, called recursively
 */

static void index_dir(struct scanner *scanner)
{
	struct duc_index_report *report = scanner->rep; 	

	report->dir_count ++;
	size_accum(&report->size, &scanner->size);

	/* Iterate directory entries. If this directory is available in the
	 * database with unchanged mtime we can iterate the database instead of
	 * the directory with a nice performance gain */

	if(scanner->buffer_prev) {
		index_dir_cached(scanner);
	} else {
		index_dir_filesystem(scanner);
	}
}



struct duc_index_report *duc_index(duc_index_req *req, const char *path, duc_index_flags flags)
{
	duc *duc = req->duc;

	req->flags = flags;

	/* Canonalize index path */

	char *path_canon = stripdir(path);
	if(path_canon == NULL) {
		duc_log(duc, DUC_LOG_WRN, "Error converting path %s: %s", path, strerror(errno));
		duc->err = DUC_E_UNKNOWN;
		if(errno == EACCES) duc->err = DUC_E_PERMISSION_DENIED;
		if(errno == ENOENT) duc->err = DUC_E_PATH_NOT_FOUND;
		return NULL;
	}

	/* Create report */
	
	struct duc_index_report *report = duc_malloc(sizeof(struct duc_index_report));
	memset(report, 0, sizeof *report);
	gettimeofday(&report->time_start, NULL);
	snprintf(report->path, sizeof(report->path), "%s", path_canon);

	/* Recursively index subdirectories */

	struct scanner *scanner = scanner_new(duc, NULL, path_canon, 0, NULL, NULL);

	if(scanner) {
		scanner->req = req;
		scanner->rep = report;
	
		req->dev = scanner->devino.dev;
		report->devino = scanner->devino;

		index_dir(scanner);
		gettimeofday(&report->time_stop, NULL);
		scanner_free(scanner);
	}
	
	/* Store report */

	gettimeofday(&report->time_stop, NULL);
	report_write(duc, report);

	free(path_canon);

	return report;
}



int duc_index_report_free(struct duc_index_report *rep)
{
	free(rep);
	return 0;
}


struct duc_index_report *duc_get_report(duc *duc, size_t id)
{
	size_t indexl;

	char *index = db_get(duc->db, "duc_index_reports", 17, &indexl);
	if(index == NULL) return NULL;

	int report_count = indexl / DUC_PATH_MAX;
	if(id >= report_count) return NULL;

	char *path = index + id * DUC_PATH_MAX;

	struct duc_index_report *r = report_read(duc, path);

	free(index);

	return r;
} 


/*
 * End
 */

