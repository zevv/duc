
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
	size_t file_count;
	size_t dir_count;
};


off_t index_dir(struct index *index, const char *path, int fd_dir, struct stat *stat_dir)
{
	off_t size_total = 0;

	int fd = openat(fd_dir, path, OPEN_FLAGS | O_NOATIME);

	if(fd == -1 && errno == EPERM) {
		fd = openat(fd_dir, path, OPEN_FLAGS);
	}

	if(fd == -1) {
		fprintf(stderr, "Error opening %s: %s\n", path, strerror(errno));
		return 0;
	}

	DIR *d = fdopendir(fd);
	if(d == NULL) {
		fprintf(stderr, "Error fdopendir %s: %s\n", path, strerror(errno));
		return 0;
	}

	struct wamb_node *node = wamb_node_new(stat_dir->st_dev, stat_dir->st_ino);

	struct dirent *e;
	while( (e = readdir(d)) != NULL) {

		const char *n = e->d_name;
		if(n[0] == '.') {
			if(n[1] == '\0') continue;
			if(n[1] == '.' && n[2] == '\0') continue;
		}

		struct stat stat;
		int r = fstatat(fd, e->d_name, &stat, AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW);
		if(r == -1) {
			fprintf(stderr, "Error statting %s: %s\n", e->d_name, strerror(errno));
			return 0;
		}
		
		if(S_ISREG(stat.st_mode) || S_ISDIR(stat.st_mode)) {

			off_t size = 0;

			if(S_ISREG(stat.st_mode)) {
				size = stat.st_size;
				index->file_count ++;
			}

			if(S_ISDIR(stat.st_mode)) {
				size = index_dir(index, e->d_name, fd, &stat);
				index->dir_count ++;
			}

			wamb_node_add_child(node, e->d_name, size, stat.st_dev, stat.st_ino);
			size_total += size;
		}
	}

	wamb_node_write(index->wamb, node);
	wamb_node_free(node);

	closedir(d);
	close(fd);

	return size_total;
}	


int wamb_index(struct wamb *wamb, const char *path, int flags)
{
	struct index index;
	memset(&index, 0, sizeof index);

	index.wamb = wamb;

	char *path_canon = realpath(path, NULL);
	if(path_canon == NULL) {
		fprintf(stderr, "Error converting path %s: %s\n", path, strerror(errno));
		return 0;
	}
	
	struct stat stat;
	int r = lstat(path_canon, &stat);
	if(r == -1) {
		fprintf(stderr, "Error statting %s: %s\n", path, strerror(errno));
		return 0;
	}

	wamb_root_write(wamb, path_canon, stat.st_dev, stat.st_ino);

	off_t size = index_dir(&index, path, 0, &stat);

	free(path_canon);

	fprintf(stderr, "Indexed %zu files and %zu directories, %jd bytes\n", 
			index.file_count, index.dir_count, size);

	return 0;
}


/*
 * End
 */

