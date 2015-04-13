
#include "config.h"

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <stdint.h>

#include "cmd.h"
#include "duc.h"
#include "ducrc.h"


static char *opt_database = NULL;
static int opt_force = 0;          
static int opt_hide_files = 0;     
static int opt_maxdepth = 0;       
static int opt_one_file_system = 0;
static int opt_uncompressed = 0;   
static duc_index_req *req;


static int index_init(duc *duc, int argc, char **argv)
{
	req = duc_index_req_new(duc);

	return 0;
}


static int index_main(duc *duc, int argc, char **argv)
{
	duc_index_flags index_flags = 0;
	int open_flags = DUC_OPEN_RW | DUC_OPEN_COMPRESS;
	
	if(opt_force) open_flags |= DUC_OPEN_FORCE;
	if(opt_maxdepth) duc_index_req_set_maxdepth(req, opt_maxdepth);
	if(opt_one_file_system) index_flags |= DUC_INDEX_XDEV;
	if(opt_hide_files) index_flags |= DUC_INDEX_HIDE_FILE_NAMES;
	if(opt_uncompressed) open_flags &= ~DUC_OPEN_COMPRESS;

	if(argc < 1) {
		duc_log(duc, DUC_LOG_WRN, "Required index PATH missing.");
		return -2;
	}
	
	int r = duc_open(duc, opt_database, open_flags);
	if(r != DUC_OK) {
		duc_log(duc, DUC_LOG_WRN, "%s", duc_strerror(duc));
		return -1;
	}

	/* Index all paths passed on the cmdline */

	int i;
	for(i=0; i<argc; i++) {

		struct duc_index_report *report;
		report = duc_index(req, argv[i], index_flags);
		if(report == NULL) {
			duc_log(duc, DUC_LOG_WRN, "%s", duc_strerror(duc));
			continue;
		}

		char *siz = duc_human_size(report->size_total);
		if(r == DUC_OK) {
			char *s = duc_human_duration(report->time_start, report->time_stop);
			duc_log(duc, DUC_LOG_INF, 
					"Indexed %lu files and %lu directories, (%sB total) in %s", 
					(unsigned long)report->file_count, 
					(unsigned long)report->dir_count,
					siz,
					s);
			free(s);
		} else {
			duc_log(duc, DUC_LOG_WRN, "An error occured while indexing: %s", duc_strerror(duc));
		}
		free(siz);

		duc_index_report_free(report);
	}

	duc_close(duc);

	return 0;
}


static void fn_exclude(const char *val)
{
	printf("exclude '%s'\n", val);
	duc_index_req_add_exclude(req, val);
}


static struct ducrc_option options[] = {
	{ &opt_database,        "database",        'd', DUCRC_TYPE_STRING, "use database file ARG" },
	{ &fn_exclude,          "exclude",         'e', DUCRC_TYPE_FUNC,   "exclude files matching ARG"  },
	{ &opt_force,           "force",           'f', DUCRC_TYPE_BOOL,   "force writing in case of corrupted db" },
	{ &opt_hide_files,      "hide-files",       0 , DUCRC_TYPE_BOOL,   "hide file names, index only directories" },
	{ &opt_maxdepth,        "maxdepth",        'm', DUCRC_TYPE_INT,    "limit directory names to given depth" },
	{ &opt_one_file_system, "one-file-system", 'x', DUCRC_TYPE_BOOL,   "don't cross filesystem boundaries" },
	{ &opt_uncompressed,    "uncompressed",     0 , DUCRC_TYPE_BOOL,   "do not use compression for database" },
	{ NULL }
};


struct cmd cmd_index = {
	.name = "index",
	.description = "Index filesystem",
	.usage = "[options] PATH ...",
	.init = index_init,
	.main = index_main,
	.options = options,
};

/*
 * End
 */

