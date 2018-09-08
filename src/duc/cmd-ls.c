
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

#define MAX_DEPTH 32

#define COLOR_RESET  "\e[0m";
#define COLOR_RED    "\e[31m";
#define COLOR_YELLOW "\e[33m";

static char *tree_ascii[] = {
	"####",
	" `+-",
	"  |-",
	"  `-",
	"  | ",
	"    ",
};


static char *tree_utf8[] = {
	"####",
	" ╰┬─",
	"  ├─",
	"  ╰─",
	"  │ ",
	"    ",
};

static bool opt_apparent = false;
static bool opt_count = false;
static bool opt_ascii = false;
static bool opt_bytes = false;
static bool opt_classify = false;
static bool opt_directory = false;
static bool opt_color = false;
static bool opt_full_path = false;
static int width = 80;
static bool opt_graph = false;
static bool opt_recursive = false;
static char *opt_database = NULL;
static bool opt_dirs_only = false;
static int opt_levels = 4;
static bool opt_name_sort = false;


/*
 * List one directory. This function is a bit hairy because of the different
 * ways the output can be formatted with optional color, trees, graphs, etc.
 * Maybe one day this should be split up for different renderings 
 */

static char parent_path[DUC_PATH_MAX] = "";
static int prefix[MAX_DEPTH + 1] = { 0 };

static void ls_one(duc_dir *dir, int level, size_t parent_path_len)
{
	off_t max_size = 0;
	size_t max_name_len = 0;
	int max_size_len = 6;
	duc_size_type st = opt_count ? DUC_SIZE_TYPE_COUNT : 
	                   opt_apparent ? DUC_SIZE_TYPE_APPARENT : DUC_SIZE_TYPE_ACTUAL;
	duc_sort sort = opt_name_sort ? DUC_SORT_NAME : DUC_SORT_SIZE;

	if(level > opt_levels) return;

	char **tree = opt_ascii ? tree_ascii : tree_utf8;

	/* Iterate the directory once to get maximum file size and name length */
	
	struct duc_dirent *e;
	while( (e = duc_dir_read(dir, st, sort)) != NULL) {

		off_t size = duc_get_size(&e->size, st);

		if(size > max_size) max_size = size;
		size_t l = strlen(e->name);
		if(l > max_name_len) max_name_len = l;
	}

	if(opt_bytes) max_size_len = 12;
	if(opt_classify) max_name_len ++;

	/* Iterate a second time to print results */

	duc_dir_rewind(dir);

	size_t count = duc_dir_get_count(dir);
	size_t n = 0;

	while( (e = duc_dir_read(dir, st, sort)) != NULL) {

		if(opt_dirs_only && e->type != DUC_FILE_TYPE_DIR) continue;

		off_t size = duc_get_size(&e->size, st);

		if(opt_recursive) {
			if(n == 0)       prefix[level] = 1;
			if(n >= 1)       prefix[level] = 2;
			if(n == count-1) prefix[level] = 3;
		}
			
		char *color_on = "";
		char *color_off = "";

		if(opt_color) {
			color_off = COLOR_RESET;
			if(size >= max_size / 8) color_on = COLOR_YELLOW;
			if(size >= max_size / 2) color_on = COLOR_RED;
		}

		printf("%s", color_on);
		char siz[32];
		duc_human_size(&e->size, st, opt_bytes, siz, sizeof siz);
		printf("%*s", max_size_len, siz);
		printf("%s", color_off);

		if(opt_recursive && !opt_full_path) {
			int *p = prefix;
			while(*p) printf("%s", tree[*p++]);
		}

		putchar(' ');

		if(opt_full_path) {
			printf("%s", parent_path);
			parent_path_len += strlen(e->name) + 1;
			if(parent_path_len + 1 < DUC_PATH_MAX) {
				strcat(parent_path, e->name);
				strcat(parent_path, "/");
			}
		}

		size_t l = printf("%s", e->name) + 1;

		if(opt_classify) {
			putchar(duc_file_type_char(e->type));
			l++;
		}

		if(opt_graph) {
			for(;l<=max_name_len; l++) putchar(' ');
			int w = width - max_name_len - max_size_len - 5;
			w -= (level + 1) * 4;
			int l = max_size ? (w * size / max_size) : 0;
			int j;
			printf(" [%s", color_on);
			for(j=0; j<l; j++) putchar('+');
			for(; j<w; j++) putchar(' ');
			printf("%s]", color_off);
		}

		putchar('\n');
			
		if(opt_recursive && level < MAX_DEPTH && e->type == DUC_FILE_TYPE_DIR) {
			if(n == count-1) {
				prefix[level] = 5;
			} else {
				prefix[level] = 4;
			}
			duc_dir *dir2 = duc_dir_openent(dir, e);
			if(dir2) {
				ls_one(dir2, level+1, parent_path_len);
				duc_dir_close(dir2);
			}
		}

		if(opt_full_path) {
			parent_path_len -= strlen(e->name) + 1;
			parent_path[parent_path_len] = '\0';
		}
		
		n++;
	}

	prefix[level] = 0;
}


/*
 * Show size of this directory only
 */

static void ls_dir_only(const char *path, duc_dir *dir)
{
	struct duc_size size;
	char siz[32];

	duc_size_type st = opt_count ? DUC_SIZE_TYPE_COUNT : 
		           opt_apparent ? DUC_SIZE_TYPE_APPARENT : DUC_SIZE_TYPE_ACTUAL;
	duc_dir_get_size(dir, &size);
	duc_human_size(&size, st, opt_bytes, siz, sizeof siz);

	printf("%s %s", siz, path);

	if(opt_classify) {
		putchar('/');
	}

	putchar('\n');
}


static int ls_main(duc *duc, int argc, char **argv)
{
	char *path = ".";
	if(argc > 0) path = argv[0];

	/* Get terminal width */

#ifdef TIOCGWINSZ
	if(isatty(STDOUT_FILENO)) {
		struct winsize w;
		int r = ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
		if(r == 0) width = w.ws_col;
	}
#endif

	/* Disable color if output is not a tty */

	if(!isatty(1)) {
		opt_color = 0;
	}

	/* Disable graph when --full-path is requested, since there is no good
	 * way to render this */

	if(opt_full_path) {
		opt_graph = 0;
	}

	/* Open database */

	int r = duc_open(duc, opt_database, DUC_OPEN_RO);
	if(r != DUC_OK) {
		return -1;
	}

	duc_dir *dir = duc_dir_open(duc, path);
	if(dir == NULL) {
		duc_log(duc, DUC_LOG_FTL, "%s", duc_strerror(duc));
		return -1;
	}

	if(opt_directory) {
		ls_dir_only(path, dir);
	} else {
		ls_one(dir, 0, 0);
	}

	duc_dir_close(dir);
	duc_close(duc);

	return 0;
}


static struct ducrc_option options[] = {
	{ &opt_apparent,  "apparent",  'a', DUCRC_TYPE_BOOL,   "show apparent instead of actual file size" },
	{ &opt_ascii,     "ascii",      0,  DUCRC_TYPE_BOOL,   "use ASCII characters instead of UTF-8 to draw tree" },
	{ &opt_bytes,     "bytes",     'b', DUCRC_TYPE_BOOL,   "show file size in exact number of bytes" },
	{ &opt_classify,  "classify",  'F', DUCRC_TYPE_BOOL,   "append file type indicator (one of */) to entries" },
	{ &opt_color,     "color",     'c', DUCRC_TYPE_BOOL,   "colorize output (only on ttys)" },
	{ &opt_count,     "count",      0,  DUCRC_TYPE_BOOL,   "show number of files instead of file size" },
	{ &opt_database,  "database",  'd', DUCRC_TYPE_STRING, "select database file to use [~/.duc.db]" },
	{ &opt_directory, "directory", 'D', DUCRC_TYPE_BOOL,   "list directory itself, not its contents" },
	{ &opt_dirs_only, "dirs-only",  0,  DUCRC_TYPE_BOOL,   "list only directories, skip individual files" },
	{ &opt_full_path, "full-path",  0,  DUCRC_TYPE_BOOL,   "show full path instead of tree in recursive view" },
	{ &opt_graph,     "graph",     'g', DUCRC_TYPE_BOOL,   "draw graph with relative size for each entry" },
	{ &opt_levels,    "levels",    'l', DUCRC_TYPE_INT,    "traverse up to ARG levels deep [4]" },
	{ &opt_name_sort, "name-sort", 'n', DUCRC_TYPE_BOOL,   "sort output by name instead of by size" },
	{ &opt_recursive, "recursive", 'R', DUCRC_TYPE_BOOL,   "recursively list subdirectories" },
	{ NULL }
};

struct cmd cmd_ls = {
	.name = "ls",
	.descr_short = "List sizes of directory",
	.usage = "[options] [PATH]",
	.main = ls_main,
	.options = options,
	.descr_long = 
		"The 'ls' subcommand queries the duc database and lists the inclusive size of\n"
		"all files and directories on the given path. If no path is given the current\n"
		"working directory is listed.\n"
};


/*
 * End
 */

