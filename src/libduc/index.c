
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


struct foo {
	int fd;
	DIR *d;
	duc_dir *dir;
	struct stat st;
	struct duc_devino devino;
	struct duc_size size;
	size_t dir_count;
	size_t file_count;
	struct duc *duc;
	struct duc_index_req *req;
	struct duc_index_report *rep;
};

	
/* 
 * Open dir and read file status 
 */

static int foo_open(struct duc *duc, struct foo *foo_parent, const char *path, struct foo *foo)
{
	int fd_parent = 0;
	
	if(foo_parent) {
		fd_parent = foo_parent->fd;
		foo->duc = foo_parent->duc;
		foo->req = foo_parent->req;
		foo->rep = foo_parent->rep;
	}

	foo->fd = openat(fd_parent, path, O_RDONLY | O_NOCTTY | O_DIRECTORY | O_NOFOLLOW);
	if(foo->fd == -1) {
		duc_log(duc, DUC_LOG_WRN, "Skipping %s: %s", path, strerror(errno));
		return -1;
	}

	int r = fstat(foo->fd, &foo->st);
	if(r == -1) {
		duc_log(duc, DUC_LOG_WRN, "Error statting %s: %s", path, strerror(errno));
		close(foo->fd);
		return -1;
	}
	
	foo->devino.dev = foo->st.st_dev;
	foo->devino.ino = foo->st.st_ino;
	
	foo->d = fdopendir(foo->fd);
	if(foo->d == NULL) {
		duc_log(duc, DUC_LOG_WRN, "Skipping %s: %s", path, strerror(errno));
		close(foo->fd);
		return -1;
	}
	
	foo->dir = duc_dir_new(duc, &foo->devino);

	if(foo_parent) {
		foo->dir->devino_parent = foo_parent->devino;
	}

	foo->file_count = 0;
	foo->dir_count = 0;
	foo->size.apparent = 0;
	foo->size.actual = 0;

	return 0;
}


void foo_close(struct foo *foo)
{
	duc_dir_close(foo->dir);
	closedir(foo->d);
}


static void index_dir(const char *path, struct foo *foo_dir, struct foo *foo_parent, int depth)
{
	struct duc *duc = foo_dir->duc;
	struct duc_index_req *req = foo_dir->req;
	struct duc_index_report *report = foo_dir->rep; 	

	duc_log(duc, DUC_LOG_DMP, ">> %s", path);


	/* Iterate directory entries */

	struct dirent *e;
	while( (e = readdir(foo_dir->d)) != NULL) {

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

		struct stat st;
		int r = fstatat(foo_dir->fd, name, &st, AT_SYMLINK_NOFOLLOW);
		if(r == -1) {
			duc_log(duc, DUC_LOG_WRN, "Error statting %s: %s", name, strerror(errno));
			continue;
		}
		 
		int type = st_to_type(st.st_mode);
		struct duc_devino devino = { .dev = st.st_dev, .ino = st.st_ino };


		/* Skip hard link duplicates for any files with more then one hard link */
		
		if((req->flags & DUC_INDEX_CHECK_HARD_LINKS) && 
		   (type != DT_DIR) && 
		   (st.st_nlink > 1) && is_duplicate(req, &devino)) 
			continue;


		/* Calculate size of this dirent, recursing when needed */

		struct duc_size ent_size = {
			.apparent = st.st_size,
			.actual = 512 * st.st_blocks,
		};
		
		report->size.apparent += ent_size.apparent;
		report->size.actual += ent_size.actual;
		
		if(type == DT_DIR) {

			if((req->flags & DUC_INDEX_XDEV) && (st.st_dev != req->dev)) {
				duc_log(duc, DUC_LOG_WRN, "Skipping %s: not crossing file system boundaries", path);
				continue;
			}

			struct foo foo_ent;
			int r = foo_open(duc, foo_dir, e->d_name, &foo_ent);
			if(r == -1) 
				continue;

			index_dir(name, &foo_ent, foo_dir, depth+1);

			ent_size.apparent += foo_ent.size.apparent;
			ent_size.actual += foo_ent.size.actual;
			foo_dir->dir_count += foo_ent.dir_count;
			foo_dir->file_count += foo_ent.file_count;
			report->dir_count ++;
			foo_dir->dir_count ++;

			foo_close(&foo_ent);
		} else {
			report->file_count ++;
			foo_dir->file_count ++;
		}
		
		foo_dir->size.apparent += ent_size.apparent;
		foo_dir->size.actual += ent_size.actual;

		duc_log(duc, DUC_LOG_DMP, "  %c %jd %jd %s", 
				typechar[type], ent_size.apparent, ent_size.actual, name);

		/* Store record */

		if(req->maxdepth == 0 || depth < req->maxdepth) {

			/* Hide file names? */

			if((req->flags & DUC_INDEX_HIDE_FILE_NAMES) && type != DT_DIR) 
				name = "<FILE>";

			duc_dir_add_ent(foo_dir->dir, name, &ent_size, type, &devino);
		}
	}

	duc_log(duc, DUC_LOG_DMP, "<< %s files:%jd dirs:%jd actual:%jd apparent:%jd", 
			path, foo_dir->file_count, foo_dir->dir_count, foo_dir->size.apparent, foo_dir->size.actual);
		
	db_write_dir(foo_dir->dir);
	
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

	/* Recursively index subdirectories */

	gettimeofday(&report->time_start, NULL);


	struct foo foo;
	foo.duc = duc;
	foo.req = req;
	foo.rep = report;
	int r = foo_open(duc, NULL, path_canon, &foo);
	if(r == 0) {

		req->dev = foo.st.st_dev;
		report->devino.dev = foo.st.st_dev;
		report->devino.ino = foo.st.st_ino;

		index_dir(path_canon, &foo, NULL, 0);
		gettimeofday(&report->time_stop, NULL);
		foo_close(&foo);
	}
	
	/* Fill in report */

	snprintf(report->path, sizeof(report->path), "%s", path_canon);
	gettimeofday(&report->time_stop, NULL);

	/* Store report */

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

