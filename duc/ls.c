
#include "config.h"

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
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

static char type_char[] = {
        [DT_BLK]     = ' ',
        [DT_CHR]     = ' ',
        [DT_DIR]     = '/',
        [DT_FIFO]    = '|',
        [DT_LNK]     = '>',
        [DT_REG]     = ' ',
        [DT_SOCK]    = '@',
        [DT_UNKNOWN] = ' ',
};

static char *tree_ascii[5] = {
	" `+-",
	"  |-",
	"  `-",
	"  | ",
	"    ",
};


static int bytes = 0;
static int classify = 0;
static int color = 0;
static int width = 80;
static int graph = 0;
static int recursive = 0;
static int max_depth = 32;
static char **tree = tree_ascii;

static void ls_one(duc_dir *dir, int level, char *prefix)
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

	if(bytes) max_size_len = 12;
	if(classify) max_name_len ++;

	/* Iterate a second time to print results */

	duc_dir_rewind(dir);

	size_t count = duc_dir_get_count(dir);
	size_t n = 0;

	while( (e = duc_dir_read(dir)) != NULL) {

		if(recursive) {
			if(n == 0)       strcpy(prefix + level*4, tree[0]);
			if(n >= 1)       strcpy(prefix + level*4, tree[1]);
			if(n == count-1) strcpy(prefix + level*4, tree[2]);
		}
			
		char *color_on = "";
		char *color_off = "";

		if(color) {
			color_off = color_reset;
			if(e->size >= max_size / 8) color_on = color_yellow;
			if(e->size >= max_size / 2) color_on = color_red;
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
		printf("%s", prefix);

		int l = printf(" %s", e->name);
		if(classify) {
			if(e->type <= DT_UNKNOWN) putchar(type_char[e->type]);
			l++;
		}

		if(graph) {
			for(;l<=max_name_len; l++) putchar(' ');
			int w = width - max_name_len - max_size_len - 5 - strlen(prefix);
			int l = max_size ? (w * e->size / max_size) : 0;
			int j;
			printf(" [%s", color_on);
			for(j=0; j<l; j++) putchar('+');
			for(; j<w; j++) putchar(' ');
			printf("%s]", color_off);
		}

		printf("\n");
			
		if(recursive && level < max_depth && e->type == DT_DIR) {
			if(n == count-1) {
				strcpy(prefix + level*4, tree[4]);
			} else {
				strcpy(prefix + level*4, tree[3]);
			}
			duc_dir *dir2 = duc_dir_openent(dir, e);
			if(dir2) {
				ls_one(dir2, level+1, prefix);
				duc_dir_close(dir2);
			}
		}
		
		n++;
	}
}


static struct option longopts[] = {
	{ "bytes",          no_argument,       NULL, 'b' },
	{ "color",          no_argument,       NULL, 'c' },
	{ "classify",       no_argument,       NULL, 'F' },
	{ "database",       required_argument, NULL, 'd' },
	{ "graph",          no_argument,       NULL, 'g' },
	{ "recursive",      no_argument,       NULL, 'R' },
	{ "verbose",        no_argument,       NULL, 'v' },
	{ NULL }
};


static int ls_main(int argc, char **argv)
{
	int c;
	char *path_db = NULL;
	duc_log_level loglevel = DUC_LOG_WRN;
	
	/* Open duc context */
	
	duc *duc = duc_new();
	if(duc == NULL) {
		duc_log(duc, DUC_LOG_WRN, "Error creating duc context");
		return -1;
	}

	while( ( c = getopt_long(argc, argv, "bcd:FgqvR", longopts, NULL)) != EOF) {

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
			case 'q':
				loglevel = DUC_LOG_FTL;
				break;
			case 'R':
				recursive = 1;
				break;
			case 'v':
				if(loglevel < DUC_LOG_DMP) loglevel ++;
				break;
			default:
				return -2;
		}
	}
	
	duc_set_log_level(duc, loglevel);

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


	int r = duc_open(duc, path_db, DUC_OPEN_RO);
	if(r != DUC_OK) {
		return -1;
	}

	duc_dir *dir = duc_dir_open(duc, path);
	if(dir == NULL) {
		duc_log(duc, DUC_LOG_WRN, "%s", duc_strerror(duc));
		return -1;
	}

	char prefix[16*4 + 1] = "";

	ls_one(dir, 0, prefix);

	duc_dir_close(dir);
	duc_close(duc);
	duc_del(duc);

	return 0;
}

static struct ducrc_option option_list[] = {
	{ "bytes",     'b', DUCRC_TYPE_BOOL,   NULL,  "show file size in exact number of bytes" },
	{ "color",     'c', DUCRC_TYPE_BOOL,   NULL,  "colorize output" },
	{ "graph",     'g', DUCRC_TYPE_BOOL,   NULL,  "draw graph with relative size for each entry" },
	{ "classify",  'F', DUCRC_TYPE_BOOL,   NULL,  "append file type indicator (one of */) to entries" },
	{ "recursive", 'R', DUCRC_TYPE_BOOL,   NULL,  "list subdirectories in a recursive tree view" },
	{ "foobar",      0, DUCRC_TYPE_STRING, "nal", "Foobar option" },
	{ NULL }
};


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
		"  -F, --classify          append indicator (one of */) to entries\n"
		"  -q, --quiet             quiet mode, do not print any warnings\n"
		"  -R, --recursive         list subdirectories in a recursive tree view\n"
		"  -v, --verbose           verbose mode, can be passed two times for debugging\n",
	.main = ls_main,
	.options = option_list,
};


/*
 * End
 */

