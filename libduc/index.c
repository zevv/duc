
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>

#include "db.h"
#include "duc.h"
#include "duc-private.h"

#define OPEN_FLAGS (O_RDONLY | O_NOCTTY | O_DIRECTORY | O_NOFOLLOW)


struct index {
	struct duc *duc;
	struct duc_index_report *report;
	int one_file_system;
	dev_t dev;
	int depth;
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


off_t index_dir(struct index *index, const char *path, int fd_dir, struct stat *stat_dir)
{
	struct duc *duc = index->duc;
	off_t size_dir = 0;

	int fd = openat(fd_dir, path, OPEN_FLAGS | O_NOATIME);

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

	struct duc_dir *dir = duc_dir_new(index->duc, 8);
			
	if(index->dev == 0) {
		index->dev = stat_dir->st_dev;
	}

	struct dirent *e;
	while( (e = readdir(d)) != NULL) {

		/* Skip . and .. */

		const char *n = e->d_name;
		if(n[0] == '.') {
			if(n[1] == '\0') continue;
			if(n[1] == '.' && n[2] == '\0') continue;
		}

		/* Get file info */

		struct stat stat;
		int r = fstatat(fd, e->d_name, &stat, AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW);
		if(r == -1) {
			duc_log(duc, LG_WRN, "Error statting %s: %s\n", e->d_name, strerror(errno));
			continue;
		}

		/* Check for file system boundaries */

		if(index->one_file_system) {
			if(stat.st_dev != index->dev) {
				duc_log(duc, LG_WRN, "Skipping %s: different file system\n", e->d_name);
				continue;
			}
		}

		/* Calculate size, recursing when needed */

		off_t size = 0;
		
		if(S_ISDIR(stat.st_mode)) {
			index->depth ++;
			size = index_dir(index, e->d_name, fd, &stat);
			index->depth --;
			index->report->dir_count ++;
		} else {
			size = stat.st_size;
			index->report->file_count ++;
		}

		duc_log(duc, LG_DBG, "%s %jd\n", e->d_name, size);

		/* Store record */

		duc_dir_add_ent(dir, e->d_name, size, mode_t_to_duc_mode(stat.st_mode), stat.st_dev, stat.st_ino);
		size_dir += size;
		index->report->size_total += size;
	}

	duc_dir_write(dir, stat_dir->st_dev, stat_dir->st_ino);
	duc_closedir(dir);

	closedir(d);

	return size_dir;
}	



int duc_index(duc *duc, const char *path, int flags, struct duc_index_report *report)
{
	struct index index;
	memset(&index, 0, sizeof index);

	/* If no report pointer was passed by the caller, use a local variable instead */

	struct duc_index_report dummy_report;
	if(report == NULL) report = &dummy_report;
	memset(report, 0, sizeof *report);

	index.duc = duc;
	index.one_file_system = flags & DUC_INDEX_XDEV;
	index.report = report;

	/* Canonalize index path */

	char *path_canon = realpath(path, NULL);
	if(path_canon == NULL) {
		duc_log(duc, LG_WRN, "Error converting path %s: %s\n", path, strerror(errno));
		duc->err = DUC_E_UNKNOWN;
		if(errno == EACCES) duc->err = DUC_E_PERMISSION_DENIED;
		if(errno == ENOENT) duc->err = DUC_E_PATH_NOT_FOUND;
		return -1;
	}

	/* Open path */
	
	struct stat stat;
	int r = lstat(path_canon, &stat);
	if(r == -1) {
		duc_log(duc, LG_WRN, "Error statting %s: %s\n", path_canon, strerror(errno));
		duc->err = DUC_E_UNKNOWN;
		if(errno == EACCES) duc->err = DUC_E_PERMISSION_DENIED;
		return -1;
	}

	/* Recursively index subdirectories */

	report->time_start = time(NULL);
	index_dir(&index, path_canon, 0, &stat);
	report->time_stop = time(NULL);
	
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

	return 0;
}


int duc_list(duc *duc, size_t id, struct duc_index_report *report)
{
	size_t indexl;

	char *index = db_get(duc->db, "duc_index_reports", 17, &indexl);
	if(index == NULL) return -1;

	int report_count = indexl / PATH_MAX;
	if(id >= report_count) return -1;

	char *path = index + id * PATH_MAX;

	size_t rlen;
	struct duc_index_report *r = db_get(duc->db, path, strlen(path), &rlen);
	if(r == NULL) {
		free(index);
		return -1;
	}
	free(index);

	if(rlen == sizeof *report) {
		memcpy(report, r, sizeof *report);
		return 0;
	}

	return -1;
} 


/*
 * End
 */

