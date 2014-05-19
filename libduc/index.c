
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
#include <fnmatch.h>

#include "db.h"
#include "duc.h"
#include "list.h"
#include "duc-private.h"

#define OPEN_FLAGS (O_RDONLY | O_NOCTTY | O_DIRECTORY | O_NOFOLLOW)



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
	char *path_canon = realpath(path, NULL);
	if(path_canon == NULL) {
		duc_log(duc, LG_WRN, "Error converting path %s: %s\n", path, strerror(errno));
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
		if(fnmatch(l->data, name, 0) == 0) return 1;
		l = l->next;
	}
	return 0;
}


static off_t index_dir(struct duc_index_req *req, struct duc_index_report *report, const char *path, int fd_dir, struct stat *stat_dir)
{
	struct duc *duc = req->duc;
	off_t size_dir = 0;

	int fd = openat(fd_dir, path, OPEN_FLAGS);

	if(fd == -1 && errno == EPERM) {
		fd = openat(fd_dir, path, OPEN_FLAGS);
	}

	if(fd == -1) {
		duc_log(duc, LG_WRN, "Skipping %s: %s\n", path, strerror(errno));
		return 0;
	}

	DIR *d = fdopendir(fd);
	if(d == NULL) {
		duc_log(duc, LG_WRN, "Skipping %s: %s\n", path, strerror(errno));
		return 0;
	}

	struct duc_dir *dir = duc_dir_new(duc, 8);
			
	if(req->dev == 0) {
		req->dev = stat_dir->st_dev;
	}

	struct dirent *e;
	while( (e = readdir(d)) != NULL) {

		/* Skip . and .. */

		const char *n = e->d_name;
		if(n[0] == '.') {
			if(n[1] == '\0') continue;
			if(n[1] == '.' && n[2] == '\0') continue;
		}

		if(req->exclude_list) {
			if(match_list(e->d_name, req->exclude_list)) continue;
		}

		/* Get file info */

		struct stat stat;
		int r = fstatat(fd, e->d_name, &stat, AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW);
		if(r == -1) {
			duc_log(duc, LG_WRN, "Error statting %s: %s\n", e->d_name, strerror(errno));
			continue;
		}

		/* Check for file system boundaries */

		if(req->flags & DUC_INDEX_XDEV) {
			if(stat.st_dev != req->dev) {
				duc_log(duc, LG_WRN, "Skipping %s: different file system\n", e->d_name);
				continue;
			}
		}

		/* Calculate size, recursing when needed */

		off_t size = 0;
		
		if(S_ISDIR(stat.st_mode)) {
			size += index_dir(req, report, e->d_name, fd, &stat);
			report->dir_count ++;
		} else {
			size = stat.st_size;
			report->file_count ++;
		}

		duc_log(duc, LG_DBG, "%s %jd\n", e->d_name, size);

		/* Store record */

		duc_dir_add_ent(dir, e->d_name, size, mode_t_to_duc_mode(stat.st_mode), stat.st_dev, stat.st_ino);
		size_dir += size;
	}
		
	duc_dir_write(dir, stat_dir->st_dev, stat_dir->st_ino);
	duc_closedir(dir);

	closedir(d);

	return size_dir;
}	


struct duc_index_report *duc_index(duc_index_req *req, const char *path, duc_index_flags flags)
{
	duc *duc = req->duc;

	req->flags = flags;

	/* Canonalize index path */

	char *path_canon = realpath(path, NULL);
	if(path_canon == NULL) {
		duc_log(duc, LG_WRN, "Error converting path %s: %s\n", path, strerror(errno));
		duc->err = DUC_E_UNKNOWN;
		if(errno == EACCES) duc->err = DUC_E_PERMISSION_DENIED;
		if(errno == ENOENT) duc->err = DUC_E_PATH_NOT_FOUND;
		return NULL;
	}

	/* Open path */
	
	struct stat stat;
	int r = lstat(path_canon, &stat);
	if(r == -1) {
		duc_log(duc, LG_WRN, "Error statting %s: %s\n", path_canon, strerror(errno));
		duc->err = DUC_E_UNKNOWN;
		if(errno == EACCES) duc->err = DUC_E_PERMISSION_DENIED;
		return NULL;
	}

	/* Create report */
	
	struct duc_index_report *report = duc_malloc(sizeof(struct duc_index_report));

	/* Recursively index subdirectories */

<<<<<<< HEAD
	gettimeofday(&report->time_start, NULL);
	report->size_total = index_dir(&index, path_canon, 0, &stat);
	gettimeofday(&report->time_stop, NULL);
=======
	report->time_start = time(NULL);
	report->size_total = index_dir(req, report, path_canon, 0, &stat);
	report->time_stop = time(NULL);
>>>>>>> 21219ebe4ab8a968481d159a6634afa7c66e6035
	
	/* Fill in report */

	snprintf(report->path, sizeof(report->path), "%s", path_canon);
	report->dev = stat.st_dev;
	report->ino = stat.st_ino;

	/* Store report. Add the report index to the 'duc_index_reports' key if
	 * not previously indexed */

	size_t tmpl;
	char *tmp = db_get(duc->db, path_canon, strlen(path_canon), &tmpl);
	if(tmp == NULL) {
		db_putcat(duc->db, "duc_index_reports", 17, report->path, sizeof report->path);
	} else {
		free(tmp);
	}

	db_put(duc->db, path_canon, strlen(path_canon), report, sizeof *report);

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

