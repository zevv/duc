
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
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

		duc_dir_add_ent(dir, e->d_name, size, stat.st_mode, stat.st_dev, stat.st_ino);
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

	static struct duc_index_report dummy_report;
	if(report == NULL) report = &dummy_report;
	memset(report, 0, sizeof *report);

	snprintf(report->path, sizeof(report->path), "%s", path);

	index.duc = duc;
	index.one_file_system = flags & DUC_INDEX_XDEV;
	index.report = report;

	char *path_canon = realpath(path, NULL);
	if(path_canon == NULL) {
		duc_log(duc, LG_WRN, "Error converting path %s: %s\n", path, strerror(errno));
		duc->err = DUC_E_UNKNOWN;
		if(errno == EACCES) duc->err = DUC_E_PERMISSION_DENIED;
		if(errno == ENOENT) duc->err = DUC_E_PATH_NOT_FOUND;
		return -1;
	}
	
	struct stat stat;
	int r = lstat(path_canon, &stat);
	if(r == -1) {
		duc_log(duc, LG_WRN, "Error statting %s: %s\n", path, strerror(errno));
		duc->err = DUC_E_UNKNOWN;
		if(errno == EACCES) duc->err = DUC_E_PERMISSION_DENIED;
		return -1;
	}

	report->dev = stat.st_dev;
	report->ino = stat.st_ino;

	/* Index directories */

	report->time_start = time(NULL);
	index_dir(&index, path_canon, 0, &stat);
	report->time_stop = time(NULL);

	/* Store report */

	db_put(duc->db, path_canon, strlen(path_canon), report, sizeof *report);

	free(path_canon);

	return 0;
}


/*
 * End
 */

