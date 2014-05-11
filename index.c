
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
#include <bsd/string.h>

#include <kclangc.h>
#include "db.h"

#define OPEN_FLAGS (O_RDONLY | O_NOCTTY | O_DIRECTORY | O_NOFOLLOW | O_NOATIME)


struct index {
	KCDB* db;
	uint32_t id_seq;
};


off_t index_dir(struct index *index, const char *path, int fd_dir, struct stat *stat_dir)
{
	off_t size_total = 0;

	int fd = openat(fd_dir, path, OPEN_FLAGS);
	if(fd == -1) {
		fprintf(stderr, "Error opening %s: %s\n", path, strerror(errno));
		return 0;
	}

	DIR *d = fdopendir(fd);
	if(d == NULL) {
		fprintf(stderr, "Error fdopendir %s: %s\n", path, strerror(errno));
		return 0;
	}

	size_t child_max = 32;
	size_t child_count = 0;
	struct db_child *child_list = malloc(child_max * sizeof(struct db_child));
	assert(child_list);

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

		/* Realloc child list if growing out of bounds */

		if(child_count >= child_max) {
			child_max = child_max * 2;
			child_list = realloc(child_list, child_max * sizeof(struct db_child));
			assert(child_list);
		}
		
		if(S_ISREG(stat.st_mode) || S_ISDIR(stat.st_mode)) {

			struct db_child *child = &child_list[child_count];
			child_count ++;

			strlcpy(child->name, e->d_name, sizeof(child->name));
			
			if(S_ISREG(stat.st_mode)) {
				child->size = stat.st_size;
				child->dev = 0;
				child->ino = 0;
			}

			if(S_ISDIR(stat.st_mode)) {
				child->size = index_dir(index, e->d_name, fd, &stat);
				child->dev = stat.st_dev;
				child->ino = stat.st_ino;
			}

			size_total += child->size;
		}
	}

	char key[32];
	snprintf(key, sizeof key, "d %jd %jd", stat_dir->st_dev, stat_dir->st_ino);
	kcdbset(index->db, key, strlen(key), (void *)child_list, child_count * sizeof(struct db_child));
	free(child_list);

	closedir(d);
	close(fd);

	return size_total;
}	




off_t ps_index(struct db *db, const char *path)
{
	struct index index;

	index.id_seq = 0;
	index.db = db->kcdb;

//	kcdbopen(index.db, "files.kch#opts=", KCOWRITER | KCOCREATE);

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

	char key[PATH_MAX + 32];
	char val[32];
	snprintf(key, sizeof key, "%s", path_canon);
	snprintf(val, sizeof val, "%jd %jd", stat.st_dev, stat.st_ino);
	kcdbset(index.db, key, strlen(key), val, strlen(val));

	off_t size = index_dir(&index, path, 0, &stat);

	free(path_canon);

	return size;
}


int dump(struct db *db, const char *path);


int main(int argc, char **argv)
{

	if(argv[1][0] == 'd') {
		struct db *db = db_open("r");
		dump(db, argv[2]);
		db_close(db);
	}

	if(argv[1][0] == 'i') {
		struct db *db = db_open("wc");
		off_t size = 0;
		size = ps_index(db, argv[2]);
		printf("%jd %.2f\n", size, size / (1024.0*1024.0*1024.0));
		db_close(db);
	}


	return 0;
}

/*
 * End
 */

