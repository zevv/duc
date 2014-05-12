
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <stdint.h>

#include "db.h"
#include "wamb.h"
#include "wamb_internal.h"

#define OPEN_FLAGS (O_RDONLY | O_NOCTTY | O_DIRECTORY | O_NOFOLLOW)


struct index {
	struct wamb *wamb;
	int one_file_system;
	int verbose;
	int quiet;
	dev_t dev;
	size_t file_count;
	size_t dir_count;
	int depth;
};


off_t index_dir(struct index *index, const char *path, int fd_dir, struct stat *stat_dir)
{
	off_t size_total = 0;

	int fd = openat(fd_dir, path, OPEN_FLAGS | O_NOATIME);

	if(fd == -1 && errno == EPERM) {
		fd = openat(fd_dir, path, OPEN_FLAGS);
	}

	if(fd == -1) {
		if(!index->quiet) 
			fprintf(stderr, "Skipping %s: %s\n", path, strerror(errno));
		return 0;
	}

	DIR *d = fdopendir(fd);
	if(d == NULL) {
		if(!index->quiet)
			fprintf(stderr, "Skipping %s: %s\n", path, strerror(errno));
		return 0;
	}

	struct wambdir *dir = wambdir_new(index->wamb, 8);
			
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
			if(!index->quiet)
				fprintf(stderr, "Error statting %s: %s\n", e->d_name, strerror(errno));
			continue;
		}

		/* Check for file system boundaries */

		if(index->one_file_system) {
			if(stat.st_dev != index->dev) {
				if(!index->quiet)
					fprintf(stderr, "Skipping %s: different file system\n", e->d_name);
				continue;
			}
		}

		/* Calculate size, recursing when needed */

		off_t size = 0;
		
		if(S_ISDIR(stat.st_mode)) {
			index->depth ++;
			size = index_dir(index, e->d_name, fd, &stat);
			index->depth --;
			index->dir_count ++;
		} else {
			size = stat.st_size;
			index->file_count ++;
		}

		if(index->verbose) {
			int j;
			for(j=0; j<index->depth; j++) putchar(' ');
			printf(" %s %jd\n", e->d_name, size);
		}

		/* Store record */

		wambdir_add_ent(dir, e->d_name, size, stat.st_mode, stat.st_dev, stat.st_ino);
		size_total += size;
	}

	wambdir_write(dir, stat_dir->st_dev, stat_dir->st_ino);
	wamb_closedir(dir);

	closedir(d);

	return size_total;
}	


int wamb_index(struct wamb *wamb, const char *path, int flags)
{
	struct index index;
	memset(&index, 0, sizeof index);

	index.wamb = wamb;
	index.one_file_system = flags & WAMB_INDEX_XDEV;
	index.verbose = flags & WAMB_INDEX_VERBOSE;
	index.quiet = flags & WAMB_INDEX_QUIET;

	char *path_canon = realpath(path, NULL);
	if(path_canon == NULL) {
		if(!index.quiet)
			fprintf(stderr, "Error converting path %s: %s\n", path, strerror(errno));
		return 0;
	}
	
	struct stat stat;
	int r = lstat(path_canon, &stat);
	if(r == -1) {
		if(!index.quiet)
			fprintf(stderr, "Error statting %s: %s\n", path, strerror(errno));
		return 0;
	}

	wamb_root_write(wamb, path_canon, stat.st_dev, stat.st_ino);

	off_t size = index_dir(&index, path_canon, 0, &stat);

	free(path_canon);

	if(!index.quiet) {
		fprintf(stderr, "Indexed %zu files and %zu directories, %jd bytes\n", 
				index.file_count, index.dir_count, size);
	}

	return 0;
}


/*
 * End
 */

