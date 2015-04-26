
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
#include <tcutil.h>
#ifdef HAVE_FNMATCH_H
#include <fnmatch.h>
#endif

#include "db.h"
#include "duc.h"
#include "list.h"
#include "private.h"

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
	TCMAP *dev_ino_map;
};

struct index_result {
	size_t file_count;
	size_t dir_count;
	struct duc_size size;
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
	req->dev_ino_map = tcmapnew();

	return req;
}


int duc_index_req_free(duc_index_req *req)
{
	tcmapdel(req->dev_ino_map);
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
	 
	int l;
	if(tcmapget(req->dev_ino_map, devino, sizeof(*devino), &l)) {
		return 1;
	}

	tcmapput(req->dev_ino_map, devino, sizeof(*devino), "", 0);
	return 0;
}


static void index_dir(struct duc_index_req *req, struct duc_index_report *report, const char *path, 
		int fd_parent, struct duc_devino *devino_parent, int depth, struct index_result *res)
{
	struct duc *duc = req->duc;
		
	duc_log(duc, DUC_LOG_DMP, ">> %s", path);

	/* Open dir and read file status */

	int fd_dir = openat(fd_parent, path, O_RDONLY | O_NOCTTY | O_DIRECTORY | O_NOFOLLOW);
	if(fd_dir == -1) {
		duc_log(duc, DUC_LOG_WRN, "Skipping %s: %s", path, strerror(errno));
		return;
	}

	struct stat st_dir;
	int r = fstat(fd_dir, &st_dir);
	if(r == -1) {
		duc_log(duc, DUC_LOG_WRN, "Error statting %s: %s", path, strerror(errno));
		close(fd_dir);
		return;
	}
	
	if(req->dev == 0) {
		req->dev = st_dir.st_dev;
	}

	struct duc_devino di_dir = { .dev = st_dir.st_dev, .ino = st_dir.st_ino };

	if(report->devino.dev == 0) {
		report->devino = di_dir;
	}


	/* Check if we are allowed to cross file system boundaries */

	if(req->flags & DUC_INDEX_XDEV) {
		if(st_dir.st_dev != req->dev) {
			duc_log(duc, DUC_LOG_WRN, "Skipping %s: not crossing file system boundaries", path);
			return;
		}
	}

	DIR *d = fdopendir(fd_dir);
	if(d == NULL) {
		duc_log(duc, DUC_LOG_WRN, "Skipping %s: %s", path, strerror(errno));
		close(fd_dir);
		return;
	}

	struct duc_dir *dir = duc_dir_new(duc, &di_dir);

	if(devino_parent) {
		dir->devino_parent = *devino_parent;
	}


	/* Iterate directory entries */

	struct dirent *e;
	while( (e = readdir(d)) != NULL) {

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
		int r = fstatat(fd_dir, name, &st, AT_SYMLINK_NOFOLLOW);
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
			struct index_result res2 = { 0 };
			struct duc_devino devino_parent = { .dev = st_dir.st_dev, .ino = st_dir.st_ino };
			index_dir(req, report, name, fd_dir, &devino_parent, depth+1, &res2);

			ent_size.apparent += res2.size.apparent;
			ent_size.actual += res2.size.actual;
			res->dir_count += res2.dir_count;
			res->file_count += res2.file_count;
			report->dir_count ++;
			res->dir_count ++;
		} else {
			report->file_count ++;
			res->file_count ++;
		}
		
		res->size.apparent += ent_size.apparent;
		res->size.actual += ent_size.actual;

		duc_log(duc, DUC_LOG_DMP, "  %c %jd %jd %s", 
				typechar[type], ent_size.apparent, ent_size.actual, name);

		/* Store record */

		if(req->maxdepth == 0 || depth < req->maxdepth) {

			/* Hide file names? */

			if((req->flags & DUC_INDEX_HIDE_FILE_NAMES) && type != DT_DIR) 
				name = "<FILE>";

			duc_dir_add_ent(dir, name, &ent_size, type, &devino);
		}
	}

	duc_log(duc, DUC_LOG_DMP, "<< %s files:%jd dirs:%jd actual:%jd apparent:%jd", 
			path, res->file_count, res->dir_count, res->size.apparent, res->size.actual);
		
	db_write_dir(dir);
	duc_dir_close(dir);

	closedir(d);
	
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

	struct index_result res = { 0 };
	index_dir(req, report, path_canon, 0, NULL, 0, &res);
	gettimeofday(&report->time_stop, NULL);
	
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

