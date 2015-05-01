
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
#include "list.h"
#include "private.h"
#include "uthash.h"

struct devino_ent {
	struct duc_devino devino;
	UT_hash_handle hh;
};

struct duc_index_req {
	duc *duc;
	struct list *path_list;
	struct list *exclude_list;
	dev_t dev;
	duc_index_flags flags;
	int maxdepth;
	duc_index_progress_cb progress_fn;
	void *progress_fndata;
	int progress_n;
	struct timeval progress_interval;
	struct timeval progress_time;
	struct devino_ent *devino_map;
};

static char typechar[] = {
	[DT_BLK] = 'b', [DT_CHR] = 'c', [DT_DIR]  = 'd', [DT_FIFO]    = 'f',
	[DT_LNK] = 'l', [DT_REG] = 'r', [DT_SOCK] = 's', [DT_UNKNOWN] = 'u',
};


duc_index_req *duc_index_req_new(duc *duc)
{
	struct duc_index_req *req = duc_malloc(sizeof(struct duc_index_req));
	memset(req, 0, sizeof *req);

	req->duc = duc;
	req->progress_interval.tv_sec = 0;
	req->progress_interval.tv_usec = 100 * 1000;
	req->devino_map = NULL;

	return req;
}


int duc_index_req_free(duc_index_req *req)
{
	struct devino_ent *de, *den;
	HASH_ITER(hh, req->devino_map, de, den) {
		HASH_DEL(req->devino_map, de);
		free(de);
	}
	list_free(req->exclude_list, free);
	list_free(req->path_list, free);
	free(req);
	return 0;
}


int duc_index_req_add_path(duc_index_req *req, const char *path)
{
	duc *duc = req->duc;
	char *path_canon = stripdir(path);
	if(path_canon == NULL) {
		duc_log(duc, DUC_LOG_WRN, "Error converting path %s: %s", path, strerror(errno));
		duc->err = DUC_E_UNKNOWN;
		if(errno == EACCES) duc->err = DUC_E_PERMISSION_DENIED;
		if(errno == ENOENT) duc->err = DUC_E_PATH_NOT_FOUND;
		return -1;
	}

	list_push(&req->path_list, path_canon);
	return 0;
}


int duc_index_req_add_exclude(duc_index_req *req, const char *patt)
{
	char *pcopy = duc_strdup(patt);
	list_push(&req->exclude_list, pcopy);
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


static int match_list(const char *name, struct list *l)
{
	while(l) {
#ifdef HAVE_FNMATCH_H
		if(fnmatch(l->data, name, 0) == 0) return 1;
#else
		if(strstr(name, l->data) == 0) return 1;
#endif
		l = l->next;
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
	/* Check if the above key is already in the map. If so, this is a dup. If not,
	 * we see this node for the first time, and we add it to the map */

	struct devino_ent *di;
	HASH_FIND(hh, req->devino_map, devino, sizeof(*devino), di);

	if(di)
		return 1;

	di = duc_malloc(sizeof *di);
	di->devino = *devino;
	HASH_ADD(hh, req->devino_map, devino, sizeof(di->devino), di);

	return 0;
}


struct scanner {
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
		free(scanner);
		return NULL;
	}

	if(st) {
		scanner->st = *st;
	} else {

		int r = fstat(scanner->fd, &scanner->st);
		if(r == -1) {
			duc_log(duc, DUC_LOG_WRN, "Error statting %s: %s", path, strerror(errno));
			close(scanner->fd);
			free(scanner);
			return NULL;
		}
	}
	
	scanner->d = fdopendir(scanner->fd);
	if(scanner->d == NULL) {
		duc_log(duc, DUC_LOG_WRN, "Skipping %s: %s", path, strerror(errno));
		close(scanner->fd);
		free(scanner);
		return NULL;
	}
	
	scanner->path = duc_strdup(path);
	scanner->duc = duc;
	scanner->size.apparent = 0;
	scanner->size.actual = 0;
	scanner->devino.dev = scanner->st.st_dev;
	scanner->devino.ino = scanner->st.st_ino;

	scanner->dir = duc_dir_new(duc, &scanner->devino);

	if(scanner_parent) {
		scanner->depth = scanner_parent->depth + 1;
		scanner->duc = scanner_parent->duc;
		scanner->req = scanner_parent->req;
		scanner->rep = scanner_parent->rep;
		scanner->dir->devino_parent = scanner_parent->devino;
	}

	return scanner;
}


static void scanner_free(struct scanner *scanner)
{
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

	duc_log(duc, DUC_LOG_DMP, ">> %s", scanner_dir->path);


	/* Iterate directory entries */

	struct dirent *e;
	while( (e = readdir(scanner_dir->d)) != NULL) {

		/* Skip . and .. */

		char *name = e->d_name;

		if(name[0] == '.') {
			if(name[1] == '\0') continue;
			if(name[1] == '.' && name[2] == '\0') continue;
		}

		if(match_list(name, req->exclude_list)) {
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

		/* Calculate size of this dirent */

		struct duc_size ent_size;
		
		if(type == DT_DIR) {

			/* Check if we can skip file system boundaries */

			if((req->flags & DUC_INDEX_XDEV) && (st_ent.st_dev != req->dev)) {
				duc_log(duc, DUC_LOG_WRN, "Skipping %s: not crossing file system boundaries", scanner_dir->path);
				continue;
			}

			/* Open child directory */

			struct scanner *scanner_ent = scanner_new(duc, scanner_dir, name, &st_ent);
			if(scanner_ent == NULL)
				continue;

			/* Recursive scan */

			index_dir(scanner_ent);

			/* Calculate totals */

			ent_size.apparent = st_ent.st_size + scanner_ent->size.apparent;
			ent_size.actual = 512 * st_ent.st_blocks + scanner_ent->size.actual;

			report->dir_count ++;
			report->size.apparent += st_ent.st_size;
			report->size.actual += 512 * st_ent.st_blocks;

			scanner_free(scanner_ent);

		} else {

			/* Skip hard link duplicates for any files with more then one hard link */
			
			if((req->flags & DUC_INDEX_CHECK_HARD_LINKS) && 
			   (st_ent.st_nlink > 1) && is_duplicate(req, &devino)) 
				continue;

			/* Hide file names? */

			if(req->flags & DUC_INDEX_HIDE_FILE_NAMES) 
				name = "<FILE>";

			ent_size.apparent = st_ent.st_size,
			ent_size.actual = 512 * st_ent.st_blocks,

			report->size.apparent += st_ent.st_size;
			report->size.actual += 512 * st_ent.st_blocks;
			report->file_count ++;
		}
		
		scanner_dir->size.apparent += ent_size.apparent;
		scanner_dir->size.actual += ent_size.actual;

		duc_log(duc, DUC_LOG_DMP, "  %c %jd %jd %s", 
				typechar[type], ent_size.apparent, ent_size.actual, name);

		/* Store record */

		if(req->maxdepth == 0 || scanner_dir->depth < req->maxdepth) 
			duc_dir_add_ent(scanner_dir->dir, name, &ent_size, type, &devino);
	}

	duc_log(duc, DUC_LOG_DMP, "<< %s actual:%jd apparent:%jd", 
			scanner_dir->path, scanner_dir->size.apparent, scanner_dir->size.actual);
		
	db_write_dir(scanner_dir->dir);
	
	/* Progress reporting */

	if(req->progress_fn) {

		if(req->progress_n++ == 100) {
			
			struct timeval t_now;
			gettimeofday(&t_now, NULL);

			if(timercmp(&t_now, &req->progress_time, > )) {
				req->progress_fn(report, req->progress_fndata);
				timeradd(&t_now, &req->progress_interval, &req->progress_time);
			}
			req->progress_n = 0;
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

	/* Final progress callback */

	if(req->progress_fn) {
		req->progress_fn(report, req->progress_fndata);
	}

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

