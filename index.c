
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>

#define OPEN_FLAGS O_RDONLY | O_NOCTTY | O_DIRECTORY | O_NOFOLLOW | O_NOATIME


off64_t subindex(const char *path, int fd_parent)
{
	struct stat stat;
	off64_t size = 0;

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

		struct dirent *e;
		while( (e = readdir(d)) != NULL) {
	
			const char *n = e->d_name;
			if(n[0] == '.') {
				if(n[1] == '\0') continue;
				if(n[1] == '.' && n[2] == '\0') continue;
			}

			size += subindex(e->d_name, fd);
		}

		closedir(d);
		close(fd);
	}

//	printf("%s %lu %jd\n", path, stat.st_ino, size);
	return size;
}


off64_t ps_index(const char *path)
{
	int fd = open(".", OPEN_FLAGS);
	off64_t size = subindex(path, fd);
	close(fd);
	return size;
}


int main(int argc, char **argv)
{
	off64_t size = 0;

	size = ps_index(argv[1]);
	printf("%jd %.2f\n", size, size / (1024.0*1024.0*1024.0));
	return 0;
}

