
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
#include "duc.h"
#include "private.h"
#include "uthash.h"
#include "utlist.h"

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
	duc_dir *dir;
	struct stat st;
	struct duc_devino devino;
	struct duc_size size;
	struct duc *duc;
	struct duc_index_req *req;
	struct duc_index_report *rep;
};

static char typechar[] = {
	[DT_BLK] = 'b', [DT_CHR] = 'c', [DT_DIR]  = 'd', [DT_FIFO]    = 'f',
	[DT_LNK] = 'l', [DT_REG] = 'r', [DT_SOCK] = 's', [DT_UNKNOWN] = 'u',
};


static void size_from_st(struct duc_size *s1, struct stat *st)
{
	s1->apparent = st->st_size;
	s1->actual = st->st_blocks * 512;
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
 * Convert st_mode to DT_* type
 */

int st_to_type(mode_t mode)
{
	int type = DT_UNKNOWN;

	if(S_ISBLK(mode))  type = DT_BLK;
	if(S_ISCHR(mode))  type = DT_CHR;
	if(S_ISDIR(mode))  type = DT_DIR;
	if(S_ISFIFO(mode)) type = DT_FIFO;
	if(S_ISLNK(mode))  type = DT_LNK;
	if(S_ISREG(mode )) type = DT_REG;
	if(S_ISSOCK(mode)) type = DT_SOCK;

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

static struct scanner *scanner_new(struct duc *duc, struct scanner *scanner_parent, const char *path, struct stat *st)
{
	struct scanner *scanner;
	scanner = duc_malloc(sizeof *scanner);

	int fd_parent = scanner_parent ? scanner_parent->fd : 0;

	scanner->fd = openat(fd_parent, path, O_RDONLY | O_NOCTTY | O_DIRECTORY | O_NOFOLLOW);
	if(scanner->fd == -1) {
		duc_log(duc, DUC_LOG_WRN, "Skipping %s: %s", path, strerror(errno));
		goto err;
	}

	if(st) {
		scanner->st = *st;
	} else {

		int r = fstat(scanner->fd, &scanner->st);
		if(r == -1) {
			duc_log(duc, DUC_LOG_WRN, "Error statting %s: %s", path, strerror(errno));
			goto err;
		}
	}
	
	scanner->d = fdopendir(scanner->fd);
	if(scanner->d == NULL) {
		duc_log(duc, DUC_LOG_WRN, "Skipping %s: %s", path, strerror(errno));
		goto err;
	}
	
	scanner->parent = scanner_parent;
	scanner->path = duc_strdup(path);
	scanner->duc = duc;
	scanner->devino.dev = scanner->st.st_dev;
	scanner->devino.ino = scanner->st.st_ino;
	size_from_st(&scanner->size, &scanner->st);

	scanner->dir = duc_dir_new(duc, &scanner->devino);

	if(scanner_parent) {
		scanner->depth = scanner_parent->depth + 1;
		scanner->duc = scanner_parent->duc;
		scanner->req = scanner_parent->req;
		scanner->rep = scanner_parent->rep;
		scanner->dir->devino_parent = scanner_parent->devino;
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
			duc_dir_add_ent(scanner->parent->dir, scanner->path, &scanner->size, DT_DIR, &scanner->devino);

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

	db_write_dir(scanner->dir);
	duc_dir_close(scanner->dir);
	closedir(scanner->d);
	duc_free(scanner->path);
	duc_free(scanner);
}


static void index_dir(struct scanner *scanner_dir)
{
	struct duc *duc = scanner_dir->duc;
	struct duc_index_req *req = scanner_dir->req;
	struct duc_index_report *report = scanner_dir->rep; 	

	report->dir_count ++;
	size_accum(&report->size, &scanner_dir->size);

	/* Iterate directory entries */

	struct dirent *e;
	while( (e = readdir(scanner_dir->d)) != NULL) {

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
		int r = fstatat(scanner_dir->fd, name, &st_ent, AT_SYMLINK_NOFOLLOW);
		if(r == -1) {
			duc_log(duc, DUC_LOG_WRN, "Error statting %s: %s", name, strerror(errno));
			continue;
		}
		 
		int type = st_to_type(st_ent.st_mode);
		struct duc_devino devino = { .dev = st_ent.st_dev, .ino = st_ent.st_ino };


		/* Skip hard link duplicates for any files with more then one hard link */

		if(type != DT_DIR && req->flags & DUC_INDEX_CHECK_HARD_LINKS && 
	 	   st_ent.st_nlink > 1 && is_duplicate(req, &devino)) {
			continue;
		}


		/* Check if we can cross file system boundaries */

		if(type == DT_DIR && req->flags & DUC_INDEX_XDEV && st_ent.st_dev != req->dev) {
			duc_log(duc, DUC_LOG_WRN, "Skipping %s: not crossing file system boundaries", name);
			continue;
		}


		/* Calculate size of this dirent */
		
		if(type == DT_DIR) {

			/* Open and scan child directory */

			struct scanner *scanner_ent = scanner_new(duc, scanner_dir, name, &st_ent);
			if(scanner_ent == NULL)
				continue;

			index_dir(scanner_ent);
			scanner_free(scanner_ent);

		} else {

			struct duc_size ent_size;
			size_from_st(&ent_size, &st_ent);
			size_accum(&scanner_dir->size, &ent_size);
			size_accum(&report->size, &ent_size);

			report->file_count ++;

			duc_log(duc, DUC_LOG_DMP, "  %c %jd %jd %s", 
					typechar[type], ent_size.apparent, ent_size.actual, name);


			/* Optionally hide file names */

			if(req->flags & DUC_INDEX_HIDE_FILE_NAMES) name = "<FILE>";
		

			/* Store record */

			if(req->maxdepth == 0 || scanner_dir->depth < req->maxdepth) 
				duc_dir_add_ent(scanner_dir->dir, name, &ent_size, type, &devino);
		}
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

	struct scanner *scanner = scanner_new(duc, NULL, path_canon, NULL);

	if(scanner) {
		scanner->req = req;
		scanner->rep = report;
	
		req->dev = scanner->st.st_dev;
		report->devino.dev = scanner->st.st_dev;
		report->devino.ino = scanner->st.st_ino;

		index_dir(scanner);
		gettimeofday(&report->time_stop, NULL);
		scanner_free(scanner);
	}
	
	/* Store report */

	gettimeofday(&report->time_stop, NULL);
	db_write_report(duc, report);

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

	int report_count = indexl / PATH_MAX;
	if(id >= report_count) return NULL;

	char *path = index + id * PATH_MAX;

	size_t rlen;
	struct duc_index_report *r = db_get(duc->db, path, strlen(path), &rlen);

	free(index);

	return r;
} 


/*
 * End
 */

