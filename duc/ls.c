
#include "config.h"

#include <limits.h>
#include <math.h>
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

char *color_reset = "\e[0m";
char *color_red = "\e[31m";
char *color_yellow = "\e[33m";

static int bytes = 0;
static int classify = 0;
static int color = 0;
static int width = 80;
static int graph = 0;


static void ls_one(duc_dir *dir, int level)
{

	off_t size_total = 0;
	off_t max_size = 0;
	int max_name_len = 0;
	int max_size_len = 6;

	/* Iterate the directory once to get maximum file size and name length */
	
	struct duc_dirent *e;
	while( (e = duc_dir_read(dir)) != NULL) {
		if(e->size > max_size) max_size = e->size;
		size_t l = strlen(e->name);
		if(l > max_name_len) max_name_len = l;
		size_total += e->size;
	}

	if(bytes && max_size_len > 0) max_size_len = log(max_size) / log(10) + 1;
	if(classify) max_name_len ++;

	/* Iterate a second time to print results */

	duc_dir_rewind(dir);
	while( (e = duc_dir_read(dir)) != NULL) {
			
		char *color_on = "";
		char *color_off = "";

		if(color) {
			color_off = color_reset;
			if(e->size >= max_size / 8) color_on = color_yellow;
			if(e->size >= max_size / 2) color_on = color_red;
		}

		if(classify) {
			if(e->mode == DUC_MODE_DIR) strcat(e->name, "/");
		}

		printf("%s", color_on);
		if(bytes) {
			printf("%*jd", max_size_len, e->size);
		} else {
			char *siz = duc_human_size(e->size);
			printf("%*s", max_size_len, siz);
			free(siz);
		}
		printf("%s", color_off);
		
		printf(" %-*s ", max_name_len, e->name);

		if(graph) {
			int w = width - max_name_len - max_size_len - 5;
			int n = max_size ? (w * e->size / max_size) : 0;
			int j;
			printf(" [%s", color_on);
			for(j=0; j<n; j++) putchar('+');
			for(; j<w; j++) putchar(' ');
			printf("%s]", color_off);
		}

		printf("\n");
	}
}


static int ls_main(int argc, char **argv)
{
	int c;
	char *path_db = NULL;

	struct option longopts[] = {
		{ "bytes",          no_argument,       NULL, 'b' },
		{ "color",          no_argument,       NULL, 'c' },
		{ "classify",       no_argument,       NULL, 'F' },
		{ "database",       required_argument, NULL, 'd' },
		{ "graph",          no_argument,       NULL, 'g' },
		{ "verbose",        no_argument,       NULL, 'v' },
		{ NULL }
	};
	
	/* Open duc context */
	
	duc *duc = duc_new();
	if(duc == NULL) {
                fprintf(stderr, "Error creating duc context\n");
                return -1;
        }

	while( ( c = getopt_long(argc, argv, "bcd:Fgv", longopts, NULL)) != EOF) {

		switch(c) {
			case 'b':
				bytes = 1;
				break;
			case 'c':
				color = 1;
				break;
			case 'd':
				path_db = optarg;
				break;
			case 'g':
				graph = 1;
				break;
			case 'F':
				classify = 1;
				break;
			case 'v':
				duc_set_log_level(duc, DUC_LOG_DBG);
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

	if(isatty(0)) {
#ifdef TIOCGWINSZ
		struct winsize w;
		int r = ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
		if(r == 0) width = w.ws_col;
#endif
	} else {
		color = 0;
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

	ls_one(dir, 0);

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
		"  -c, --color             colorize the output.\n"
		"  -d, --database=ARG      use database file ARG [~/.duc.db]\n"
		"  -g, --graph             draw graph with relative size for each entry\n"
		"  -h, --human-readable    print sizes in human readable format\n"
		"  -F, --classify          append indicator (one of */) to entries\n",
	.main = ls_main
};


/*
 * End
 */

