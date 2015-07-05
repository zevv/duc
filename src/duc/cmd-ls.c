
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

static int opt_apparent = 0;
static int opt_ascii = 0;
static int opt_bytes = 0;
static int opt_classify = 0;
static int opt_color = 0;
static int width = 80;
static int opt_graph = 0;
static int opt_recursive = 0;
static char *opt_database = NULL;
static int opt_dirs_only = 0;
static int opt_levels = 4;

static void ls_one(duc_dir *dir, int level, int *prefix)
{
	off_t max_size = 0;
	size_t max_name_len = 0;
	int max_size_len = 6;
	duc_size_type st = opt_apparent ? DUC_SIZE_TYPE_APPARENT : DUC_SIZE_TYPE_ACTUAL;

	if(level > opt_levels) return;

	char **tree = opt_ascii ? tree_ascii : tree_utf8;

	/* Iterate the directory once to get maximum file size and name length */
	
	struct duc_dirent *e;
	while( (e = duc_dir_read(dir, st)) != NULL) {

		off_t size = opt_apparent ? e->size.apparent : e->size.actual;

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

	while( (e = duc_dir_read(dir, st)) != NULL) {

		if(opt_dirs_only && e->type != DUC_FILE_TYPE_DIR) continue;

		off_t size = opt_apparent ? e->size.apparent : e->size.actual;

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

		int *p = prefix;
		while(*p) printf("%s", tree[*p++]);

		size_t l = printf(" %s", e->name);
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

		printf("\n");
			
		if(opt_recursive && level < MAX_DEPTH && e->type == DUC_FILE_TYPE_DIR) {
			if(n == count-1) {
				prefix[level] = 5;
			} else {
				prefix[level] = 4;
			}
			duc_dir *dir2 = duc_dir_openent(dir, e);
			if(dir2) {
				ls_one(dir2, level+1, prefix);
				duc_dir_close(dir2);
			}
		}
		
		n++;
	}

	prefix[level] = 0;
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

	int r = duc_open(duc, opt_database, DUC_OPEN_RO);
	if(r != DUC_OK) {
		return -1;
	}

	duc_dir *dir = duc_dir_open(duc, path);
	if(dir == NULL) {
		duc_log(duc, DUC_LOG_FTL, "%s", duc_strerror(duc));
		return -1;
	}

	int prefix[MAX_DEPTH + 1] = { 0 };

	ls_one(dir, 0, prefix);

	duc_dir_close(dir);
	duc_close(duc);

	return 0;
}


static struct ducrc_option options[] = {
	{ &opt_apparent,  "apparent",  'a', DUCRC_TYPE_BOOL,   "show apparent instead of actual file size" },
	{ &opt_ascii,     "ascii",       0, DUCRC_TYPE_BOOL,   "use ASCII characters instead of UTF-8 to draw tree" },
	{ &opt_bytes,     "bytes",     'b', DUCRC_TYPE_BOOL,   "show file size in exact number of bytes" },
	{ &opt_classify,  "classify",  'F', DUCRC_TYPE_BOOL,   "append file type indicator (one of */) to entries" },
	{ &opt_color,     "color",     'c', DUCRC_TYPE_BOOL,   "colorize output (only on ttys)" },
	{ &opt_database,  "database",  'd', DUCRC_TYPE_STRING, "select database file to use [~/.duc.db]" },
	{ &opt_dirs_only, "dirs-only",   0, DUCRC_TYPE_BOOL,   "list only directories, skip individual files" },
	{ &opt_graph,     "graph",     'g', DUCRC_TYPE_BOOL,   "draw graph with relative size for each entry" },
	{ &opt_levels,    "levels",    'l', DUCRC_TYPE_INT,    "traverse up to ARG levels deep [4]" },
	{ &opt_recursive, "recursive", 'R', DUCRC_TYPE_BOOL,   "list subdirectories in a recursive tree view" },
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

