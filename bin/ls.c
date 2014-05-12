
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
#include "wamb.h"
#include "wamb_internal.h"


static int human_readable = 0;
static int limit = 0;


char *human_size(off_t size)
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

	struct option longopts[] = {
		{ "database",       required_argument, NULL, 'd' },
		{ "human-readable", no_argument,       NULL, 'h' },
		{ "limit",          required_argument, NULL, 'n' },
		{ NULL }
	};

	while( ( c = getopt_long(argc, argv, "d:hn:", longopts, NULL)) != EOF) {

		switch(c) {
			case 'd':
				path_db = optarg;
				break;
			case 'h':
				human_readable = 1;
				break;
			case 'n':
				limit = atoi(optarg);
				break;
			default:
				return(-1);
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

	/* Open wamb */

	struct wamb *wamb = wamb_open(path_db, WAMB_OPEN_RO);

	wambdir *dir = wamb_opendir(wamb, path);
	if(dir == NULL) {
		fprintf(stderr, "Path not found in database\n");
		return -1;
	}
	
	/* Calculate max and total size */
	
	off_t size_total = 0;
	off_t size_max = 0;

	struct wambent *e;
	while( (e = wamb_readdir(dir)) != NULL) {
		if(e->size > size_max) size_max = e->size;
		size_total += e->size;
	}

	wamb_rewinddir(dir);
	while( (e = wamb_readdir(dir)) != NULL) {

		char *siz = human_size(e->size);
		printf("%-20.20s %s ", e->name, siz);
		free(siz);

		int n = width * e->size / size_max;
		int j;
		for(j=0; j<n; j++) putchar('#');
		printf("\n");
	}

	wamb_closedir(dir);
	wamb_close(wamb);

	return 0;
}



struct cmd cmd_ls = {
	.name = "ls",
	.description = "List directory",
	.usage = "[options] [PATH]",
	.help = 
		"Valid options:\n"
		"\n"
		"  -d, --database=ARG      use database file ARG\n"
		"  -h, --human-readable    print sizes in human readable format\n"
		"  -n, --limit=ARG         limit number of results\n",
	.main = ls_main
};


/*
 * End
 */

