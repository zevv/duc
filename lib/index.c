
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

struct duc_index_req {
	duc *duc;
	struct list *path_list;
	struct list *exclude_list;
	dev_t dev;
	duc_index_flags flags;
};


static duc_dirent_mode mode_t_to_duc_mode(mode_t m)
{
	if(S_ISREG(m))  return DUC_MODE_REG;
	if(S_ISDIR(m))  return DUC_MODE_DIR;
	if(S_ISCHR(m))  return DUC_MODE_CHR;
	if(S_ISBLK(m))  return DUC_MODE_BLK;
	if(S_ISFIFO(m)) return DUC_MODE_FIFO;
	if(S_ISLNK(m))  return DUC_MODE_LNK;
	if(S_ISSOCK(m)) return DUC_MODE_SOCK;
	return DUC_MODE_REST;
}



duc_index_req *duc_index_req_new(duc *duc)
{
	struct duc_index_req *req = duc_malloc(sizeof(struct duc_index_req));
	memset(req, 0, sizeof *req);

	req->duc = duc;

	return req;
}


int duc_index_req_free(duc_index_req *req)
{
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
		duc_log(duc, DUC_LOG_WRN, "Error converting path %s: %s\n", path, strerror(errno));
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


static off_t index_dir(struct duc_index_req *req, struct duc_index_report *report, const char *path, struct stat *st_dir, struct stat *st_parent, int depth)
{
	struct duc *duc = req->duc;
	off_t size_dir = 0;

	int r = chdir(path);
	if(r == -1) {
		duc_log(duc, DUC_LOG_WRN, "Skipping %s: %s\n", path, strerror(errno));
		return 0;
	}

	DIR *d = opendir(".");
	if(d == NULL) {
		duc_log(duc, DUC_LOG_WRN, "Skipping %s: %s\n", path, strerror(errno));
		return 0;
	}

	struct duc_dir *dir = duc_dir_new(duc, st_dir->st_dev, st_dir->st_ino);
	if(st_parent) {
		dir->dev_parent = st_parent->st_dev;
		dir->ino_parent = st_parent->st_ino;
	}
			
	if(req->dev == 0) {
		req->dev = st_dir->st_dev;
	}

	struct dirent *e;
	while( (e = readdir(d)) != NULL) {

		/* Skip . and .. */

		const char *n = e->d_name;
		if(n[0] == '.') {
			if(n[1] == '\0') continue;
			if(n[1] == '.' && n[2] == '\0') continue;
		}

		if(match_list(e->d_name, req->exclude_list)) continue;

		/* Get file info */

		struct stat st;
		int r = lstat(e->d_name, &st);
		if(r == -1) {
			duc_log(duc, DUC_LOG_WRN, "Error statting %s: %s\n", e->d_name, strerror(errno));
			continue;
		}

		/* Check for file system boundaries */

		if(req->flags & DUC_INDEX_XDEV) {
			if(st.st_dev != req->dev) {
				duc_log(duc, DUC_LOG_WRN, "Skipping %s: different file system\n", e->d_name);
				continue;
			}
		}

		/* Calculate size, recursing when needed */

		off_t size = 0;
		
		if(S_ISDIR(st.st_mode)) {
			size += index_dir(req, report, e->d_name, &st, st_dir, depth-1);
			dir->dir_count ++;
			report->dir_count ++;
		} else {
			size = st.st_size;
			dir->file_count ++;
			report->file_count ++;
		}

		duc_log(duc, DUC_LOG_DMP, "%s %jd\n", e->d_name, size);

		/* Store record */

		if (depth > 0) {		/* cut entries at given depth */
			if ((req->flags & DUC_INDEX_HIDE) && !S_ISDIR(st.st_mode)) {	/* hide file names if necessary */
				duc_dir_add_ent(dir, "<FILES>", size, mode_t_to_duc_mode(st.st_mode), st.st_dev, st.st_ino);
			} else {
				duc_dir_add_ent(dir, e->d_name, size, mode_t_to_duc_mode(st.st_mode), st.st_dev, st.st_ino);
			}
		}
		dir->size_total += size;
		size_dir += size;
	}
		
	db_write_dir(dir);
	duc_dir_close(dir);

	closedir(d);
	chdir("..");

	return size_dir;
}	


struct duc_index_report *duc_index(duc_index_req *req, const char *path, duc_index_flags flags, int maxdepth)
{
	duc *duc = req->duc;

	req->flags = flags;

	/* Canonalize index path */

	char *path_canon = stripdir(path);
	if(path_canon == NULL) {
		duc_log(duc, DUC_LOG_WRN, "Error converting path %s: %s\n", path, strerror(errno));
		duc->err = DUC_E_UNKNOWN;
		if(errno == EACCES) duc->err = DUC_E_PERMISSION_DENIED;
		if(errno == ENOENT) duc->err = DUC_E_PATH_NOT_FOUND;
		return NULL;
	}

	/* Open path */
	
	struct stat st;
	int r = lstat(path_canon, &st);
	if(r == -1) {
		duc_log(duc, DUC_LOG_WRN, "Error statting %s: %s\n", path_canon, strerror(errno));
		duc->err = DUC_E_UNKNOWN;
		if(errno == EACCES) duc->err = DUC_E_PERMISSION_DENIED;
		return NULL;
	}

	/* Create report */
	
	struct duc_index_report *report = duc_malloc(sizeof(struct duc_index_report));
	memset(report, 0, sizeof *report);

	/* Recursively index subdirectories */

	gettimeofday(&report->time_start, NULL);
	report->size_total = index_dir(req, report, path_canon, &st, NULL, maxdepth);
	gettimeofday(&report->time_stop, NULL);
	
	/* Fill in report */

	snprintf(report->path, sizeof(report->path), "%s", path_canon);
	report->dev = st.st_dev;
	report->ino = st.st_ino;

	/* Store report */

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

