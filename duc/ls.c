
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "cmd.h"
#include "duc.h"


static int human_readable = 0;
static int limit = 20;


char *fmt_size(off_t size)
{
	char prefix[] = { '\0', 'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y' };
	double v = size;
	char *p = prefix;
	char *s = NULL;

	if(!human_readable || size < 1024) {
		int r = asprintf(&s, "%10jd", size);
		if(r != -1) return s;
	} else {
		while(v >= 1024.0) {
			v /= 1024.0;
			p ++;
		}
		int r = asprintf(&s, "%9.1f%c", v, *p);
		if(r != -1) return s;
	}

	return NULL;
}


static int ls_main(int argc, char **argv)
{
	int c;
	char *path_db = NULL;
	int classify = 0;

	struct option longopts[] = {
		{ "classify",       no_argument,       NULL, 'F' },
		{ "database",       required_argument, NULL, 'd' },
		{ "human-readable", no_argument,       NULL, 'h' },
		{ "limit",          required_argument, NULL, 'n' },
		{ NULL }
	};

	while( ( c = getopt_long(argc, argv, "d:Fhn:", longopts, NULL)) != EOF) {

		switch(c) {
			case 'd':
				path_db = optarg;
				break;
			case 'F':
				classify = 1;
				break;
			case 'h':
				human_readable = 1;
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
	if(isatty(0)) {
		struct winsize w;
		int r = ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
		if(r == 0) width = w.ws_col;
	}
	width = width - 34;

	/* Open duc context */

	struct duc *duc = duc_open(path_db, DUC_OPEN_RO);

	ducdir *dir = duc_opendir(duc, path);
	if(dir == NULL) {
		fprintf(stderr, "Path not found in database\n");
		return -1;
	}
	
	/* Calculate max and total size */
	
	off_t size_total = 0;
	off_t size_max = 0;

	struct ducent *e;
	while( (e = duc_readdir(dir)) != NULL) {
		if(e->size > size_max) size_max = e->size;
		size_total += e->size;
	}

	int count = 0;
	off_t size_rest = 0;
	size_t count_rest = 0;

	duc_rewinddir(dir);
	while( (e = duc_readdir(dir)) != NULL) {

		count ++;

		if(!limit || count < limit) {

			if(classify) {
				if(S_ISDIR(e->mode)) strcat(e->name, "/");
				if(S_ISREG(e->mode) && (e->mode & (S_IXUSR | S_IXGRP | S_IXOTH))) strcat(e->name, "*");
			}

			char *siz = fmt_size(e->size);
			printf("%-20.20s %s ", e->name, siz);
			free(siz);

			int n = size_max ? (width * e->size / size_max) : 0;
			int j;
			for(j=0; j<n; j++) putchar('#');
			printf("\n");
		} else {
			size_rest += e->size;
			count_rest ++;
		}
	}

	if(count_rest) {
		char *siz = fmt_size(size_rest);
		printf("\n");
		printf("%-20.20s %s\n", "Omitted files", siz);
		free(siz);
	}

	duc_closedir(dir);
	duc_close(duc);

	return 0;
}



struct cmd cmd_ls = {
	.name = "ls",
	.description = "List directory",
	.usage = "[options] [PATH]",
	.help = 
		"Valid options:\n"
		"\n"
		"  -d, --database=ARG      use database file ARG [~/.duc.db]\n"
		"  -h, --human-readable    print sizes in human readable format\n"
		"  -F, --classify          append indicator (one of */) to entries\n"
		"  -n, --limit=ARG         limit number of results. 0 for unlimited [20]\n",
	.main = ls_main
};


/*
 * End
 */

