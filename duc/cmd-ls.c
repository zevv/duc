
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
static int opt_levels = 4;

static void ls_one(duc_dir *dir, int level, int *prefix)
{
	off_t size_total = 0;
	off_t max_size = 0;
	int max_name_len = 0;
	int max_size_len = 6;

	if(level > opt_levels) return;

	char **tree = opt_ascii ? tree_ascii : tree_utf8;

	/* Iterate the directory once to get maximum file size and name length */
	
	struct duc_dirent *e;
	while( (e = duc_dir_read(dir)) != NULL) {

		off_t size = opt_apparent ? e->size_apparent : e->size_actual;

		if(size > max_size) max_size = size;
		size_t l = strlen(e->name);
		if(l > max_name_len) max_name_len = l;
		size_total += size;
	}

	if(opt_bytes) max_size_len = 12;
	if(opt_classify) max_name_len ++;

	/* Iterate a second time to print results */

	duc_dir_rewind(dir);

	size_t count = duc_dir_get_count(dir);
	size_t n = 0;

	while( (e = duc_dir_read(dir)) != NULL) {

		off_t size = opt_apparent ? e->size_apparent : e->size_actual;

		if(opt_recursive) {
			if(n == 0)       prefix[level] = 1;
			if(n >= 1)       prefix[level] = 2;
			if(n == count-1) prefix[level] = 3;
		}
			
		char *color_on = "";
		char *color_off = "";

		if(opt_color) {
			color_off = color_reset;
			if(size >= max_size / 8) color_on = color_yellow;
			if(size >= max_size / 2) color_on = color_red;
		}

		printf("%s", color_on);
		if(opt_bytes) {
			printf("%*jd", max_size_len, (intmax_t)size);
		} else {
			char *siz = duc_human_size(size);
			printf("%*s", max_size_len, siz);
			free(siz);
		}
		printf("%s", color_off);

		int *p = prefix;
		while(*p) printf("%s", tree[*p++]);

		int l = printf(" %s", e->name);
		if(opt_classify && e->type < sizeof(type_char)) {
			char c = type_char[e->type];
			if(c) {
				putchar(c);
				l++;
			}
		}

		if(opt_graph) {
			for(;l<=max_name_len; l++) putchar(' ');
			int w = width - max_name_len - max_size_len - 5 - (level +1 ) * 4;
			int l = max_size ? (w * size / max_size) : 0;
			int j;
			printf(" [%s", color_on);
			for(j=0; j<l; j++) putchar('+');
			for(; j<w; j++) putchar(' ');
			printf("%s]", color_off);
		}

		printf("\n");
			
		if(opt_recursive && level < MAX_DEPTH && e->type == DT_DIR) {
			if(n == count-1) {
				prefix[level] = 5;
			} else {
				prefix[level] = 4;
			}
			duc_dir *dir2 = duc_dir_openent(dir, e, opt_apparent ? DUC_SIZE_TYPE_APPARENT : DUC_SIZE_TYPE_ACTUAL);
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

	if(isatty(0)) {
#ifdef TIOCGWINSZ
		struct winsize w;
		int r = ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
		if(r == 0) width = w.ws_col;
#endif
	} else {
		opt_color = 0;
	}


	int r = duc_open(duc, opt_database, DUC_OPEN_RO);
	if(r != DUC_OK) {
		return -1;
	}

	duc_dir *dir = duc_dir_open(duc, path, opt_apparent ? DUC_SIZE_TYPE_APPARENT : DUC_SIZE_TYPE_ACTUAL);
	if(dir == NULL) {
		return -1;
	}

	int prefix[MAX_DEPTH + 1] = { 0 };

	ls_one(dir, 0, prefix);

	duc_dir_close(dir);
	duc_close(duc);

	return 0;
}


static struct ducrc_option options[] = {
	{ &opt_apparent,  "apparent",  'a', DUCRC_TYPE_BOOL,   "Show apparent instead of actual file size" },
	{ &opt_ascii,     "ascii",       0, DUCRC_TYPE_BOOL,   "use ASCII characters instead of UTF-8 to draw tree" },
	{ &opt_bytes,     "bytes",     'b', DUCRC_TYPE_BOOL,   "show file size in exact number of bytes" },
	{ &opt_classify,  "classify",  'F', DUCRC_TYPE_BOOL,   "append file type indicator (one of */) to entries" },
	{ &opt_color,     "color",     'c', DUCRC_TYPE_BOOL,   "colorize output" },
	{ &opt_database,  "database",  'd', DUCRC_TYPE_STRING, "select database file to use [~/.duc.db]" },
	{ &opt_graph,     "graph",     'g', DUCRC_TYPE_BOOL,   "draw graph with relative size for each entry" },
	{ &opt_levels,    "levels",    'l', DUCRC_TYPE_INT,    "traverse up to ARG levels deep [4]" },
	{ &opt_recursive, "recursive", 'R', DUCRC_TYPE_BOOL,   "list subdirectories in a recursive tree view" },
	{ NULL }
};

struct cmd cmd_ls = {
	.name = "ls",
	.description = "List directory",
	.usage = "[options] [PATH]",
	.main = ls_main,
	.options = options,
};


/*
 * End
 */

