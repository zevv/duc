#include "config.h"

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

#include "cmd.h"
#include "duc.h"
	

static bool opt_apparent = false;
static char *opt_database = NULL;
static double opt_min_size = 0;
static bool opt_exclude_files = false;


static void dump(duc *duc, duc_dir *dir, struct duc_size *size_out)
{
	struct duc_dirent *e;

	duc_size_type st = opt_apparent ? DUC_SIZE_TYPE_APPARENT : DUC_SIZE_TYPE_ACTUAL;

	while( (e = duc_dir_read(dir, DUC_SIZE_TYPE_ACTUAL, DUC_SORT_SIZE)) != NULL) {

		size_out->count += 1;

		if(e->type == DUC_FILE_TYPE_DIR) {
			duc_dir *dir_child = duc_dir_openent(dir, e);
			if(dir_child) {
				dump(duc, dir_child, size_out);
			}
			duc_dir_close(dir_child);
			size_out->apparent += 0;
			size_out->actual += 4096;
		} else {
			size_out->apparent += e->size.apparent;
			size_out->actual += e->size.actual;
		}
	}
}


static int verify_main(duc *duc, int argc, char **argv)
{
	char *path = ".";
	if(argc > 0) path = argv[0];

	int r = duc_open(duc, opt_database, DUC_OPEN_RO);
	if(r != DUC_OK) {
		duc_log(duc, DUC_LOG_FTL, "%s", duc_strerror(duc));
		return -1;
	}

	duc_dir *dir = duc_dir_open(duc, path);
	if(dir == NULL) {
		duc_log(duc, DUC_LOG_FTL, "%s", duc_strerror(duc));
		return -1;
	}

	struct duc_size size1;
	duc_dir_get_size(dir, &size1);

	struct duc_size size2 = { 0, 0 };
	dump(duc, dir, &size2);

	printf("count=\"%jd\" size_apparent=\"%jd\" size_actual=\"%jd\" />\n", 
			(intmax_t)size1.count, (intmax_t)size1.apparent, (intmax_t)size1.actual);
	
	printf("count=\"%jd\" size_apparent=\"%jd\" size_actual=\"%jd\" />\n", 
			(intmax_t)size2.count, (intmax_t)size2.apparent, (intmax_t)size2.actual);
	

	duc_dir_close(dir);
	duc_close(duc);

	return 0;
}


static struct ducrc_option options[] = {
	{ &opt_apparent,      "apparent",      'a', DUCRC_TYPE_BOOL,   "interpret min_size/-s value as apparent size" },
	{ &opt_database,      "database",      'd', DUCRC_TYPE_STRING, "select database file to use [~/.duc.db]" },
	{ &opt_exclude_files, "exclude-files", 'x', DUCRC_TYPE_BOOL,   "exclude file from verify output, only include directories" },
	{ &opt_min_size,      "min_size",      's', DUCRC_TYPE_DOUBLE, "specify min size for files or directories" },
	{ NULL }
};


struct cmd cmd_verify = {
	.name = "verify",
	.descr_short = "Dump XML output",
	.usage = "[options] [PATH]",
	.main = verify_main,
	.options = options,
};


/*
 * End
 */
