
#include "config.h"

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#include "cmd.h"
#include "duc.h"


static int bytes = 0;
static int limit = 20;


static int ls_main(int argc, char **argv)
{
	int c;
	char *path_db = NULL;
	int classify = 0;

	struct option longopts[] = {
		{ "bytes",          no_argument,       NULL, 'b' },
		{ "classify",       no_argument,       NULL, 'F' },
		{ "database",       required_argument, NULL, 'd' },
		{ "limit",          required_argument, NULL, 'n' },
		{ NULL }
	};

	while( ( c = getopt_long(argc, argv, "bd:Fn:", longopts, NULL)) != EOF) {

		switch(c) {
			case 'b':
				bytes = 1;
				break;
			case 'd':
				path_db = optarg;
				break;
			case 'F':
				classify = 1;
				break;
			case 'n':
				limit = atoi(optarg);
				break;
			default:
				return -2;
		}
	}

	argc -= optind;
	argv += optind;
	
	char *path = ".";
	if(argc > 0) path = argv[0];
	
	/* Get terminal width */

	int width = 80;
#ifdef TIOCGWINSZ
	if(isatty(0)) {
		struct winsize w;
		int r = ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
		if(r == 0) width = w.ws_col;
	}
#endif
	width = width - 36;

	/* Open duc context */
	
	duc *duc = duc_new();
	if(duc == NULL) {
                fprintf(stderr, "Error creating duc context\n");
                return -1;
        }

	path_db = duc_pick_db_path(path_db);
	int r = duc_open(duc, path_db, DUC_OPEN_RO);
	if(r != DUC_OK) {
	  fprintf(stderr, "%s\n", duc_strerror(duc));
		return -1;
	}
	printf("Reading %s\n",path_db);

	duc_dir *dir = duc_dir_open(duc, path);
	if(dir == NULL) {
	  fprintf(stderr, "%s\n", duc_strerror(duc));
		return -1;
	}
	
	/* Calculate max and total size */
	
	off_t size_total = 0;
	off_t size_max = 0;

	struct duc_dirent *e;
	while( (e = duc_dir_read(dir)) != NULL) {
		if(e->size > size_max) size_max = e->size;
		size_total += e->size;
	}

	if(limit) duc_dir_limit(dir, limit);
	
	duc_dir_rewind(dir);
	while( (e = duc_dir_read(dir)) != NULL) {

		if(classify) {
			if(e->mode == DUC_MODE_DIR) strcat(e->name, "/");
		}

		char *siz;
		if(bytes) {
			asprintf(&siz, "%jd", e->size);
		} else {
			siz = duc_human_size(e->size);
		}

		printf("%-20.20s %11.11s [", e->name, siz);
		free(siz);

		int n = size_max ? (width * e->size / size_max) : 0;
		int j;
		for(j=0; j<n; j++) putchar('=');
		for(; j<width; j++) putchar(' ');
		printf("]\n");
	}

	char *siz;
	if(bytes) {
		asprintf(&siz, "%jd", duc_dir_get_size(dir));
	} else {
		siz = duc_human_size(duc_dir_get_size(dir));
	}
	printf("%-20.20s %11.11s\n", "Total size", siz);
	free(siz);

	duc_dir_close(dir);
	duc_close(duc);
	duc_del(duc);

	return 0;
}



struct cmd cmd_ls = {
	.name = "ls",
	.description = "List directory",
	.usage = "[options] [PATH]",
	.help = 
		"  -b, --bytes             show file size in exact number of bytes\n"
		"  -d, --database=ARG      use database file ARG [~/.duc.db]\n"
		"  -h, --human-readable    print sizes in human readable format\n"
		"  -F, --classify          append indicator (one of */) to entries\n"
		"  -n, --limit=ARG         limit number of results. 0 for unlimited [20]\n",
	.main = ls_main
};


/*
 * End
 */

