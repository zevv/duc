
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


static int bytes = 0;
static int limit = 20;


char *fmt_size(off_t size)
{
	char prefix[] = { '\0', 'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y' };
	double v = size;
	char *p = prefix;
	char *s = NULL;

	if(bytes || size < 1024) {
		int r = asprintf(&s, "%11jd", size);
		if(r != -1) return s;
	} else {
		while(v >= 1024.0) {
			v /= 1024.0;
			p ++;
		}
		int r = asprintf(&s, "%10.1f%c", v, *p);
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
	if(isatty(0)) {
		struct winsize w;
		int r = ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
		if(r == 0) width = w.ws_col;
	}
	width = width - 36;

	/* Open duc context */

	duc_errno err;
	struct duc *duc = duc_open(path_db, DUC_OPEN_RO, &err);
	if(duc == NULL) {
		fprintf(stderr, "%s\n", duc_strerror(err));
		return -1;
	}

	duc_dir *dir = duc_opendir(duc, path);
	if(dir == NULL) {
		fprintf(stderr, "%s\n", duc_strerror(duc_error(duc)));
		return -1;
	}
	
	/* Calculate max and total size */
	
	off_t size_total = 0;
	off_t size_max = 0;

	struct duc_dirent *e;
	while( (e = duc_readdir(dir)) != NULL) {
		if(e->size > size_max) size_max = e->size;
		size_total += e->size;
	}

	if(limit) duc_limitdir(dir, limit);
	
	duc_rewinddir(dir);
	while( (e = duc_readdir(dir)) != NULL) {

		if(classify) {
			if(S_ISDIR(e->mode)) strcat(e->name, "/");
			if(S_ISREG(e->mode) && (e->mode & (S_IXUSR | S_IXGRP | S_IXOTH))) strcat(e->name, "*");
		}

		char siz[32];
		if(bytes) {
			snprintf(siz, sizeof siz, "%jd", e->size);
		} else {
			duc_format_size(e->size, siz, sizeof siz);
		}

		printf("%-20.20s %11.11s [", e->name, siz);

		int n = size_max ? (width * e->size / size_max) : 0;
		int j;
		for(j=0; j<n; j++) putchar('=');
		for(; j<width; j++) putchar(' ');
		printf("]\n");
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

