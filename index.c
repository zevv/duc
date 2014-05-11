
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

#define OPEN_FLAGS (O_RDONLY | O_NOCTTY | O_DIRECTORY | O_NOFOLLOW | O_NOATIME)


struct index {
	KCDB* db;
	uint32_t id_seq;
};

struct child {
	char name[64];
	off_t size;
};


static void store_dir(struct index *index, const char *path, off_t size, struct child *child_list, int child_count)
{
	int i;
	char buf[1024*1024];
	int l = 0;

	l += snprintf(buf+l, sizeof(buf)-l, "%jd\n", size);

	for(i=0; i<child_count; i++) {
		struct child *child = &child_list[i];
		l += snprintf(buf+l, sizeof(buf)-l, "%s\n%jd\n", child->name, child->size);
	}

	kcdbset(index->db, path, strlen(path), buf, l);

	//printf("%s %jd %d\n", path, size, buflen);
}


off_t subindex(struct index *index, const char *path, int fd_parent)
{
	struct stat stat;
	off_t size = 0;

	int r = fstatat(fd_parent, path, &stat, AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW);
	if(r == -1) {
		fprintf(stderr, "Error statting %s: %s\n", path, strerror(errno));
		return 0;
	}

	if(S_ISREG(stat.st_mode)) {
		size = stat.st_size;
	}

	if(S_ISDIR(stat.st_mode)) {
	
		int fd = openat(fd_parent, path, OPEN_FLAGS);
		if(fd == -1) {
			fprintf(stderr, "Error opening %s: %s\n", path, strerror(errno));
			return 0;
		}

		DIR *d = fdopendir(fd);
		if(d == NULL) {
			fprintf(stderr, "Error fdopendir %s: %s\n", path, strerror(errno));
			return 0;
		}

		int child_count = 0;
		int child_max = 32;
		struct child *child_list = malloc(child_max * sizeof(struct child));
		assert(child_list);

		struct dirent *e;
		while( (e = readdir(d)) != NULL) {
	
			const char *n = e->d_name;
			if(n[0] == '.') {
				if(n[1] == '\0') continue;
				if(n[1] == '.' && n[2] == '\0') continue;
			}

			if(child_count >= child_max) {
				child_max *= 2;
				child_list = realloc(child_list, child_max * sizeof(struct child));
				assert(child_list);
			}

			struct child *child = &child_list[child_count];
			strlcpy(child->name, e->d_name, sizeof(child->name));
			child->size = subindex(index, e->d_name, fd);
			child_count ++;

			size += child->size;
		}

		store_dir(index, path, size, child_list, child_count);
		free(child_list);

		closedir(d);
		close(fd);
		//printf("%d: %s %jd\n", id, path, size);
	}

	return size;
}


off_t ps_index(const char *path)
{
	struct index index;

	index.id_seq = 0;
	index.db = kcdbnew();

	kcdbopen(index.db, "files.kch#opts=", KCOWRITER | KCOCREATE);

	char *path_canon = realpath(path, NULL);
	if(path_canon == NULL) {
		fprintf(stderr, "Error converting path %s: %s\n", path, strerror(errno));
		return 0;
	}

	int fd = open(path_canon, OPEN_FLAGS);
	if(fd == -1) {
		fprintf(stderr, "Error opening %s: %s\n", path, strerror(errno));
		return 0;
	}

	off_t size = subindex(&index, path, fd);

	free(path_canon);
	close(fd);

	return size;
}


int dump(const char *path);


int main(int argc, char **argv)
{


	if(argv[1][0] == 'd') {
		dump(argv[2]);
	}

	if(argv[1][0] == 'i') {
		off_t size = 0;
		size = ps_index(argv[2]);
		printf("%jd %.2f\n", size, size / (1024.0*1024.0*1024.0));
	}

	return 0;
}

/*
 * End
 */

