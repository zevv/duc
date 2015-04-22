
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
	off_t size_actual;
	off_t size_apparent;
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



static off_t index_dir(struct duc_index_req *req, struct duc_index_report *report, const char *path, int fd_parent, struct stat *st_parent, int depth, struct index_result *res)
{
	struct duc *duc = req->duc;
		
	duc_log(duc, DUC_LOG_DMP, ">> %s", path);

	/* Open dir and read file status */

	int fd_dir = openat(fd_parent, path, O_RDONLY | O_NOCTTY | O_DIRECTORY | O_NOFOLLOW);
	if(fd_dir == -1) {
		duc_log(duc, DUC_LOG_WRN, "Skipping %s: %s", path, strerror(errno));
		return 0;
	}

	struct stat st_dir;
	int r = fstat(fd_dir, &st_dir);
	if(r == -1) {
		duc_log(duc, DUC_LOG_WRN, "Error statting %s: %s", path, strerror(errno));
		close(fd_dir);
		return 0;
	}
	
	if(req->dev == 0) {
		req->dev = st_dir.st_dev;
	}

	if(report->dev == 0) {
		report->dev = st_dir.st_dev;
		report->ino = st_dir.st_ino;
	}


	/* Check if we are allowed to cross file system boundaries */

	if(req->flags & DUC_INDEX_XDEV) {
		if(st_dir.st_dev != req->dev) {
			duc_log(duc, DUC_LOG_WRN, "Skipping %s: not crossing file system boundaries", path);
			return 0;
		}
	}

	DIR *d = fdopendir(fd_dir);
	if(d == NULL) {
		duc_log(duc, DUC_LOG_WRN, "Skipping %s: %s", path, strerror(errno));
		close(fd_dir);
		return 0;
	}

	struct duc_dir *dir = duc_dir_new(duc, st_dir.st_dev, st_dir.st_ino);

	if(st_parent) {
		dir->dev_parent = st_parent->st_dev;
		dir->ino_parent = st_parent->st_ino;
	}


	/* Iterate directory entries */

	struct dirent *e;
	while( (e = readdir(d)) != NULL) {

		/* Skip . and .. */

		const char *n = e->d_name;
		if(n[0] == '.') {
			if(n[1] == '\0') continue;
			if(n[1] == '.' && n[2] == '\0') continue;
		}

		if(match_list(e->d_name, req->exclude_list)) {
			duc_log(duc, DUC_LOG_WRN, "Skipping %s: excluded by user", e->d_name);
			continue;
		}

		/* Get file info */

		struct stat st;
		int r = fstatat(fd_dir, e->d_name, &st, AT_SYMLINK_NOFOLLOW);
		if(r == -1) {
			duc_log(duc, DUC_LOG_WRN, "Error statting %s: %s", e->d_name, strerror(errno));
			continue;
		}

		/* Check for hard link duplicates in hash table */

		if(req->flags & DUC_INDEX_CHECK_HARD_LINKS) {

			struct { dev_t dev; ino_t ino; } key = { st.st_dev, st.st_ino };
			int l;

			const void *ptr = tcmapget(req->dev_ino_map, &key, sizeof(key), &l);

			if(ptr)
				continue;

			tcmapput(req->dev_ino_map, &key, sizeof(key), "", 0);
		}

		/* Find out the file type from st.st_mode. It seems that we can
		 * not trust e->d_type because it is not guaranteed to contain
		 * a sane value on all file system types. See the readdir() man
		 * page for more details */
		 
		unsigned char type = DT_UNKNOWN;

		if(S_ISBLK(st.st_mode))  type = DT_BLK;
		if(S_ISCHR(st.st_mode))  type = DT_CHR;
		if(S_ISDIR(st.st_mode))  type = DT_DIR;
		if(S_ISFIFO(st.st_mode)) type = DT_FIFO;
		if(S_ISLNK(st.st_mode))  type = DT_LNK;
		if(S_ISREG(st.st_mode )) type = DT_REG;
		if(S_ISSOCK(st.st_mode)) type = DT_SOCK;

		/* Calculate size of this dirent, recursing when needed */

		off_t ent_size_apparent = st.st_size;
		off_t ent_size_actual = 512 * st.st_blocks;
		
		if(type == DT_DIR) {
			struct index_result res2 = { 0 };
			index_dir(req, report, e->d_name, fd_dir, &st_dir, depth+1, &res2);

			ent_size_apparent += res2.size_apparent;
			ent_size_actual += res2.size_actual;
			res->dir_count += res2.dir_count;
			res->file_count += res2.file_count;
			res->dir_count ++;
		} else {
			res->file_count ++;
		}
		
		res->size_apparent += ent_size_apparent;
		res->size_actual += ent_size_actual;

		duc_log(duc, DUC_LOG_DMP, "  %c %jd %jd %s", 
				typechar[type], ent_size_apparent, ent_size_actual, e->d_name);

		/* Store record */

		if(req->maxdepth == 0 || depth < req->maxdepth) {

			char *name = e->d_name;
		
			/* Hide file names? */

			if((req->flags & DUC_INDEX_HIDE_FILE_NAMES) && type != DT_DIR) {
				name = "<FILE>";
			}

			duc_dir_add_ent(dir, name, ent_size_apparent, ent_size_actual, type, st.st_dev, st.st_ino);
		}


	}

	duc_log(duc, DUC_LOG_DMP, "<< %s files:%jd dirs:%jd actual:%jd apparent:%jd", 
			path, res->file_count, res->dir_count, res->size_apparent, res->size_actual);
		
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

	return 0;
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
	report->size_apparent = res.size_apparent;
	report->size_actual = res.size_actual;
	report->dir_count = res.dir_count;
	report->file_count = res.file_count;
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

