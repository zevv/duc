
#include "config.h"

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <wchar.h>
#include <locale.h>
#include <unistd.h>

#include "cmd.h"
#include "duc.h"


static bool opt_apparent = false;
static bool opt_count = false;
static bool opt_bytes = false;
static char *opt_database = NULL;
static size_t opt_bin_min = 1;
static size_t opt_bin_max = 4294967296ULL;
static double opt_bin_exp = 2;


static void do_one(struct duc *duc, const char *path)
{
	duc_dir *dir = duc_dir_open(duc, path);
	if(dir == NULL) {
		if(duc_error(duc) == DUC_E_PATH_NOT_FOUND) {
			duc_log(duc, DUC_LOG_FTL, "The requested path '%s' was not found in the database,", path);
			duc_log(duc, DUC_LOG_FTL, "Please run 'duc info' for a list of available directories.");
		} else {
			duc_log(duc, DUC_LOG_FTL, "%s", duc_strerror(duc));
		}
		exit(1);
	}

	duc_size_type st = opt_count ? DUC_SIZE_TYPE_COUNT : 
	                   opt_apparent ? DUC_SIZE_TYPE_APPARENT : DUC_SIZE_TYPE_ACTUAL;

	struct duc_dirent *e;

	struct duc_histogram *h = duc_histogram_new(dir, st, opt_bin_min, opt_bin_max, opt_bin_exp);
	for(size_t i=0; i<h->bins; i++) {
		struct duc_histogram_bin *bin = &h->bin[i];
		char size_min[32];
		char size_max[32];
		char file_count[32];
		char dir_count[32];
		struct duc_size s;

		s.actual = bin->min;
		s.apparent = bin->min;
		duc_human_size(&s, st, opt_bytes, size_min, sizeof size_min);

		s.actual = bin->max;
		s.apparent = bin->max;
		duc_human_size(&s, st, opt_bytes, size_max, sizeof size_max);

		duc_human_number(bin->file_count, 0, file_count, sizeof file_count);
		duc_human_number(bin->dir_count, 0, dir_count, sizeof dir_count);
		printf("%7s .. %-7s :  files: %7s  dirs: %7s\n", size_min, size_max, file_count, dir_count);
	}
	duc_histogram_free(h);

	duc_dir_close(dir);
}


static int histogram_main(duc *duc, int argc, char **argv)
{
	/* Open database */

	int r = duc_open(duc, opt_database, DUC_OPEN_RO);
	if(r != DUC_OK) {
		return -1;
	}

	if(argc > 0) {
		int i;
		for(i=0; i<argc; i++) {
			do_one(duc, argv[i]);
		}
	} else {
		do_one(duc, ".");
	}

	duc_close(duc);

	return 0;
}


static struct ducrc_option options[] = {
	{ &opt_apparent,  "apparent",  'a', DUCRC_TYPE_BOOL,   "show apparent instead of actual file size" },
	{ &opt_bytes,     "bytes",     'b', DUCRC_TYPE_BOOL,   "show file size in exact number of bytes" },
	{ &opt_database,  "database",  'd', DUCRC_TYPE_STRING, "select database file to use [~/.duc.db]" },
	{ &opt_bin_min,  "bin-min",      0, DUCRC_TYPE_SIZE_T, "minimum size of first bin [1]" },
	{ &opt_bin_max,  "bin-max",      0, DUCRC_TYPE_SIZE_T, "maximum size of last bin [4294967296]" },
	{ &opt_bin_exp,  "bin-exp",      0, DUCRC_TYPE_DOUBLE, "exponent for bin size [2.0]" },
	{ NULL }
};

struct cmd cmd_histogram = {
	.name = "histogram",
	.descr_short = "Create a histogram of file sizes",
	.usage = "[options] [PATH]...",
	.main = histogram_main,
	.options = options,
	.descr_long =
		"The 'histogram' subcommand queries the duc database and prints a histogram of file sizes.\n"
};


/*
 * End
 */

