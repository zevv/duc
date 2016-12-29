
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
#include "buffer.h"

struct hard_link {
	struct duc_devino devino;
	UT_hash_handle hh;
};

struct fstype {
	char *path;
	char *type;
	UT_hash_handle hh;
};

struct exclude {
	char *name;
	struct exclude *next;
};

struct duc_index_req {
	duc *duc;
	struct exclude *exclude_list;
	duc_dev_t dev;
	duc_index_flags flags;
	int maxdepth;
	duc_index_progress_cb progress_fn;
	void *progress_fndata;
	int progress_n;
	struct timeval progress_interval;
	struct timeval progress_time;
	struct hard_link *hard_link_map;
	struct fstype *fstypes_mounted;
	struct fstype *fstypes_include;
	struct fstype *fstypes_exclude;
};

struct scanner {
	struct scanner *parent;
	int depth;
	DIR *d;
	struct buffer *buffer;
	struct duc *duc;
	struct duc_index_req *req;
	struct duc_index_report *rep;
	struct duc_dirent ent;
};


static void scanner_free(struct scanner *scanner);


duc_index_req *duc_index_req_new(duc *duc)
{
	struct duc_index_req *req = duc_malloc0(sizeof(struct duc_index_req));

	req->duc = duc;
	req->progress_interval.tv_sec = 0;
	req->progress_interval.tv_usec = 100 * 1000;
	req->hard_link_map = NULL;

	return req;
}


int duc_index_req_free(duc_index_req *req)
{
	struct hard_link *h, *hn;
	struct fstype *f, *fn;
	struct exclude *e, *en;

	HASH_ITER(hh, req->hard_link_map, h, hn) {
		HASH_DEL(req->hard_link_map, h);
		free(h);
	}
	
	HASH_ITER(hh, req->fstypes_mounted, f, fn) {
		duc_free(f->type);
		duc_free(f->path);
		HASH_DEL(req->fstypes_mounted, f);
		free(f);
	}
	
	HASH_ITER(hh, req->fstypes_include, f, fn) {
		duc_free(f->type);
		HASH_DEL(req->fstypes_include, f);
		free(f);
	}
	
	HASH_ITER(hh, req->fstypes_exclude, f, fn) {
		duc_free(f->type);
		HASH_DEL(req->fstypes_exclude, f);
		free(f);
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


static struct fstype *add_fstype(duc_index_req *req, const char *types, struct fstype *list)
{
	char *types_copy = duc_strdup(types);
	char *type = strtok(types_copy, ",");
	while(type) {
		struct fstype *fstype;
		fstype = duc_malloc(sizeof *fstype);
		fstype->type = duc_strdup(type);
		HASH_ADD_KEYPTR(hh, list, fstype->type, strlen(fstype->type), fstype);
		type = strtok(NULL, ",");
	}
	duc_free(types_copy);
	return list;
}


int duc_index_req_add_fstype_include(duc_index_req *req, const char *types)
{
	req->fstypes_include = add_fstype(req, types, req->fstypes_include);
	return 0;
}


int duc_index_req_add_fstype_exclude(duc_index_req *req, const char *types)
{
	req->fstypes_exclude = add_fstype(req, types, req->fstypes_exclude);
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
	if(S_ISBLK(mode))  return DUC_FILE_TYPE_BLK;
	if(S_ISCHR(mode))  return DUC_FILE_TYPE_CHR;
	if(S_ISDIR(mode))  return DUC_FILE_TYPE_DIR;
	if(S_ISFIFO(mode)) return DUC_FILE_TYPE_FIFO;
	if(S_ISLNK(mode))  return DUC_FILE_TYPE_LNK;
	if(S_ISREG(mode )) return DUC_FILE_TYPE_REG;
	if(S_ISSOCK(mode)) return DUC_FILE_TYPE_SOCK;

	return DUC_FILE_TYPE_UNKNOWN;
}


static void st_to_size(struct stat *st, struct duc_size *s)
{
	s->apparent = st->st_size;
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
	s->actual = st->st_blocks * 512;
#else
	s->actual = s->apparent;
#endif
	s->count = 1;
}


/*
 * Conver struct stat to duc_devino. Windows does not support inodes
 * and will always put 0 in st_ino. We fake inodes here by simply using
 * an incrementing counter. This *will* cause problems when re-indexing
 * existing databases. If anyone knows a better method to simulate
 * inodes on windows, please tell me
 */

static void st_to_devino(struct stat *st, struct duc_devino *devino)
{
#ifdef WIN32
	static duc_ino_t ino_seq = 0;
	devino->dev = st->st_dev;
	devino->ino = ++ino_seq;
#else
	devino->dev = st->st_dev;
	devino->ino = st->st_ino;
#endif
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


static void report_skip(struct duc *duc, const char *name, const char *fmt, ...)
{
	char path_full[DUC_PATH_MAX];
	char msg[DUC_PATH_MAX + 128];
	realpath(name, path_full);
	va_list va;
	va_start(va, fmt);
	vsnprintf(msg, sizeof(msg), fmt, va);
	duc_log(duc, DUC_LOG_WRN, "skipping %s: %s", path_full, msg);
	va_end(va);
}


/*
 * Check if this file system type should be scanned, depending on the
 * fstypes_include and fstypes_exclude lists. If neither has any entries, all
 * fs types are allowed. return 0 to skip, or 1 to scan
 */

static int is_fstype_allowed(struct duc_index_req *req, const char *name)
{
	struct duc *duc = req->duc;

	if(req->fstypes_include == NULL && req->fstypes_exclude == NULL) {
		return 1;
	}

	/* Find file system type */

	char path_full[DUC_PATH_MAX];
	realpath(name, path_full);
	struct fstype *fstype = NULL;
	HASH_FIND_STR(req->fstypes_mounted, path_full, fstype);
	if(fstype == NULL) {
		report_skip(duc, name, "Unable to determine fs type");
		return 0;
	}
	const char *type = fstype->type;

	/* Check if excluded */

	if(req->fstypes_exclude) {
		HASH_FIND_STR(req->fstypes_exclude, type, fstype);
		if(fstype) {
			report_skip(duc, name, "File system type '%s' is excluded", type);
			return 0;
		}
	}

	/* Check if included */

	if(req->fstypes_include) {
		HASH_FIND_STR(req->fstypes_include, type, fstype);
		if(!fstype) {
			report_skip(duc, name, "File system type '%s' is not included", type);
			return 0;
		}
	}

	return 1;
}


/* 
 * Open dir and read file status 
 */

static struct scanner *scanner_new(struct duc *duc, struct scanner *scanner_parent, const char *path, struct stat *st)
{
	struct scanner *scanner;
	scanner = duc_malloc0(sizeof *scanner);

	struct stat st2;
	struct duc_devino devino_parent = { 0, 0 };

	if(scanner_parent) {
		scanner->depth = scanner_parent->depth + 1;
		scanner->duc = scanner_parent->duc;
		scanner->req = scanner_parent->req;
		scanner->rep = scanner_parent->rep;
		devino_parent = scanner_parent->ent.devino;
	} else {
		int r = lstat(path, &st2);
		if(r == -1) {
			duc_log(duc, DUC_LOG_WRN, "Error statting %s: %s", path, strerror(errno));
			goto err;
		}
		st = &st2;
	}
	
	scanner->d = opendir(path);
	if(scanner->d == NULL) {
		report_skip(duc, path, strerror(errno));
		goto err;
	}
	
	scanner->duc = duc;
	scanner->parent = scanner_parent;
	scanner->buffer = buffer_new(NULL, 32768);

	scanner->ent.name = duc_strdup(path);
	scanner->ent.type = DUC_FILE_TYPE_DIR,
	st_to_devino(st, &scanner->ent.devino);
	st_to_size(st, &scanner->ent.size);

		
	buffer_put_dir(scanner->buffer, &devino_parent, st->st_mtime);

	duc_log(duc, DUC_LOG_DMP, ">> %s", scanner->ent.name);

	return scanner;

err:
	if(scanner->d) closedir(scanner->d);
	if(scanner) free(scanner);
	return NULL;
}


static void scanner_scan(struct scanner *scanner_dir)
{
	struct duc *duc = scanner_dir->duc;
	struct duc_index_req *req = scanner_dir->req;
	struct duc_index_report *report = scanner_dir->rep; 	

	report->dir_count ++;
	duc_size_accum(&report->size, &scanner_dir->ent.size);

	int r = chdir(scanner_dir->ent.name);
	if(r != 0) {
		report_skip(duc, scanner_dir->ent.name, strerror(errno));
		return;
	}

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
			report_skip(duc, name, "Excluded by user");
			continue;
		}

		/* Get file info. Derive the file type from st.st_mode. It
		 * seems that we cannot trust e->d_type because it is not
		 * guaranteed to contain a sane value on all file system types.
		 * See the readdir() man page for more details */

		struct stat st_ent;
		int r = lstat(name, &st_ent);
		if(r == -1) {
			duc_log(duc, DUC_LOG_WRN, "Error statting %s: %s", name, strerror(errno));
			continue;
		}

		/* If this dirent lies on a different device, check the file system type of the new
		 * device and skip if it is not on the list of approved types */

		if(st_ent.st_dev != scanner_dir->ent.devino.dev) {
			if(!is_fstype_allowed(req, name)) {
				continue;
			}
		}

		/* Create duc_dirent from readdir() and fstatat() results */
		
		struct duc_dirent ent;
		ent.name = name;
		ent.type = st_to_type(st_ent.st_mode);
		st_to_devino(&st_ent, &ent.devino);
		st_to_size(&st_ent, &ent.size);

		/* Skip hard link duplicates for any files with more then one hard link */

		if(ent.type != DUC_FILE_TYPE_DIR && req->flags & DUC_INDEX_CHECK_HARD_LINKS && 
	 	   st_ent.st_nlink > 1 && is_duplicate(req, &ent.devino)) {
			continue;
		}


		/* Check if we can cross file system boundaries */

		if(ent.type == DUC_FILE_TYPE_DIR && req->flags & DUC_INDEX_XDEV && st_ent.st_dev != req->dev) {
			report_skip(duc, name, "Not crossing file system boundaries");
			continue;
		}


		/* Calculate size of this dirent */
		
		if(ent.type == DUC_FILE_TYPE_DIR) {

			/* Open and scan child directory */

			struct scanner *scanner_ent = scanner_new(duc, scanner_dir, name, &st_ent);
			if(scanner_ent == NULL)
				continue;

			scanner_scan(scanner_ent);
			scanner_free(scanner_ent);

		} else {

			duc_size_accum(&scanner_dir->ent.size, &ent.size);
			duc_size_accum(&report->size, &ent.size);

			report->file_count ++;

			duc_log(duc, DUC_LOG_DMP, "  %c %jd %jd %s", 
					duc_file_type_char(ent.type), ent.size.apparent, ent.size.actual, name);


			/* Optionally hide file names */

			if(req->flags & DUC_INDEX_HIDE_FILE_NAMES) ent.name = "<FILE>";
		

			/* Store record */

			if(req->maxdepth == 0 || scanner_dir->depth < req->maxdepth) {
				buffer_put_dirent(scanner_dir->buffer, &ent);
			}
		}
	}

	chdir("..");
}


static void scanner_free(struct scanner *scanner)
{
	struct duc *duc = scanner->duc;
	struct duc_index_req *req = scanner->req;
	struct duc_index_report *report = scanner->rep; 	
	
	duc_log(duc, DUC_LOG_DMP, "<< %s actual:%jd apparent:%jd", 
			scanner->ent.name, scanner->ent.size.apparent, scanner->ent.size.actual);

	if(scanner->parent) {
		duc_size_accum(&scanner->parent->ent.size, &scanner->ent.size);

		if(req->maxdepth == 0 || scanner->depth < req->maxdepth) {
			buffer_put_dirent(scanner->parent->buffer, &scanner->ent);
		}

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
	
	char key[32];
	struct duc_devino *devino = &scanner->ent.devino;
	size_t keyl = snprintf(key, sizeof(key), "%jx/%jx", (uintmax_t)devino->dev, (uintmax_t)devino->ino);
	int r = db_put(duc->db, key, keyl, scanner->buffer->data, scanner->buffer->len);
	if(r != 0) duc->err = r;

	buffer_free(scanner->buffer);
	closedir(scanner->d);
	duc_free(scanner->ent.name);
	duc_free(scanner);
}


static void read_mounts(duc_index_req *req)
{
	FILE *f;

	f = fopen("/proc/mounts", "r");

	if(f == NULL) {
		f = fopen("/etc/mtab", "r");
	}

	if(f == NULL) {
		duc_log(req->duc, DUC_LOG_FTL, "Unable to get list of mounted file systems");
	}

	char buf[DUC_PATH_MAX];

	while(fgets(buf, sizeof(buf)-1, f) != NULL) {
		(void)strtok(buf, " ");
		char *path = strtok(NULL, " ");
		char *type = strtok(NULL, " ");
		if(path && type) {
			struct fstype *fstype;
			fstype = duc_malloc(sizeof *fstype);
			fstype->type = duc_strdup(type);
			fstype->path = duc_strdup(path);
			HASH_ADD_KEYPTR(hh, req->fstypes_mounted, fstype->path, strlen(fstype->path), fstype);
		}
	}
	fclose(f);
}


struct duc_index_report *duc_index(duc_index_req *req, const char *path, duc_index_flags flags)
{
	duc *duc = req->duc;

	req->flags = flags;

	/* Canonicalize index path */

	char *path_canon = duc_canonicalize_path(path);
	if(path_canon == NULL) {
		duc_log(duc, DUC_LOG_WRN, "Error converting path %s: %s", path, strerror(errno));
		duc->err = DUC_E_UNKNOWN;
		if(errno == EACCES) duc->err = DUC_E_PERMISSION_DENIED;
		if(errno == ENOENT) duc->err = DUC_E_PATH_NOT_FOUND;
		return NULL;
	}

	/* Create report */
	
	struct duc_index_report *report = duc_malloc0(sizeof(struct duc_index_report));
	gettimeofday(&report->time_start, NULL);
	snprintf(report->path, sizeof(report->path), "%s", path_canon);

	/* Read mounted file systems to find fs types */

	if(req->fstypes_include || req->fstypes_exclude) {
		read_mounts(req);
	}

	/* Recursively index subdirectories */

	struct scanner *scanner = scanner_new(duc, NULL, path_canon, NULL);

	if(scanner) {
		scanner->req = req;
		scanner->rep = report;
	
		req->dev = scanner->ent.devino.dev;
		report->devino = scanner->ent.devino;

		scanner_scan(scanner);
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


/*
 * End
 */

