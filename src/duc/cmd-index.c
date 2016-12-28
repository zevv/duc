
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


static int opt_bytes = 0;
static char *opt_database = NULL;
static int opt_force = 0;          
static int opt_check_hard_links = 0;
static int opt_hide_file_names = 0;     
static int opt_max_depth = 0;       
static int opt_one_file_system = 0;
static int opt_progress = 0;
static int opt_uncompressed = 0;   
static duc_index_req *req;


static int index_init(duc *duc, int argc, char **argv)
{
	req = duc_index_req_new(duc);

	return 0;
}


void progress_cb(struct duc_index_report *rep, void *ptr)
{
	static int n = 0;
	char meter[] = "--------";
	int i = 7 - abs(n-7);
	meter[i] = '#';
	n = (n+1) % 14;

	char siz[32], fs[32], ds[32];
	duc_human_size(&rep->size, DUC_SIZE_TYPE_ACTUAL, opt_bytes, siz, sizeof siz);
	duc_human_number(rep->file_count, opt_bytes, fs, sizeof fs);
	duc_human_number(rep->dir_count, opt_bytes, ds, sizeof ds);

	printf("\e[K[%s] Indexed %sb in %s files and %s directories\r", meter, siz, fs, ds);
	fflush(stdout);
}


static void log_callback(duc_log_level level, const char *fmt, va_list va)
{
	vfprintf(stderr, fmt, va);
	fprintf(stderr, "\e[K\n");
}


static int index_main(duc *duc, int argc, char **argv)
{
	duc_index_flags index_flags = 0;
	int open_flags = DUC_OPEN_RW | DUC_OPEN_COMPRESS;
	
	if(opt_force) open_flags |= DUC_OPEN_FORCE;
	if(opt_max_depth) duc_index_req_set_maxdepth(req, opt_max_depth);
	if(opt_one_file_system) index_flags |= DUC_INDEX_XDEV;
	if(opt_hide_file_names) index_flags |= DUC_INDEX_HIDE_FILE_NAMES;
	if(opt_check_hard_links) index_flags |= DUC_INDEX_CHECK_HARD_LINKS;
	if(opt_uncompressed) open_flags &= ~DUC_OPEN_COMPRESS;

	if(argc < 1) {
		duc_log(duc, DUC_LOG_FTL, "Required index PATH missing.");
		return -2;
	}
	
	if(opt_progress) {
		duc_index_req_set_progress_cb(req, progress_cb, NULL);
		duc_set_log_callback(duc, log_callback);
	}
	
	int r = duc_open(duc, opt_database, open_flags);
	if(r != DUC_OK) {
		duc_log(duc, DUC_LOG_FTL, "%s", duc_strerror(duc));
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

		char siz_apparent[32], siz_actual[32];
		duc_human_size(&report->size, DUC_SIZE_TYPE_APPARENT, opt_bytes, siz_apparent, sizeof siz_apparent);
		duc_human_size(&report->size, DUC_SIZE_TYPE_ACTUAL,   opt_bytes, siz_actual,   sizeof siz_actual);

		if(r == DUC_OK) {
			char dur[64];
			duc_human_duration(report->time_start, report->time_stop, dur, sizeof dur);
			duc_log(duc, DUC_LOG_INF, 
					"Indexed %zu files and %zu directories, (%sB apparent, %sB actual) in %s", 
					report->file_count, 
					report->dir_count,
					siz_apparent,
					siz_actual,
					dur);
		} else {
			duc_log(duc, DUC_LOG_WRN, "An error occurred while indexing: %s", duc_strerror(duc));
		}

		/* Prevent final output of progress_cb() from being overwritten with the shell's prompt */
		if (opt_progress) {
			fputc ('\n', stdout);
			fflush (stdout);
		}

		duc_index_report_free(report);
	}

	duc_close(duc);
	duc_index_req_free(req);

	return 0;
}


static void fn_exclude(const char *val)
{
	duc_index_req_add_exclude(req, val);
}


static void fn_fs_include(const char *val)
{
	duc_index_req_add_fstype_include(req, val);
}


static void fn_fs_exclude(const char *val)
{
	duc_index_req_add_fstype_exclude(req, val);
}


static struct ducrc_option options[] = {
	{ &opt_bytes,           "bytes",           'b', DUCRC_TYPE_BOOL,   "show file size in exact number of bytes" },
	{ &opt_database,        "database",        'd', DUCRC_TYPE_STRING, "use database file VAL" },
	{ fn_exclude,           "exclude",         'e', DUCRC_TYPE_FUNC,   "exclude files matching VAL"  },
	{ &opt_check_hard_links,"check-hard-links",'H', DUCRC_TYPE_BOOL,   "count hard links only once",
          "if two or more hard links point to the same file, only one of the hard links is displayed and counted" },
	{ &opt_force,           "force",           'f', DUCRC_TYPE_BOOL,   "force writing in case of corrupted db" },
	{ fn_fs_exclude,        "fs-exclude",       0,  DUCRC_TYPE_FUNC,   "exclude file system type VAL during indexing",
	  "VAL is a comma separated list of file system types as found in your systems fstab, for example ext3,ext4,dosfs" },
	{ fn_fs_include,        "fs-include",       0,  DUCRC_TYPE_FUNC,   "include file system type VAL during indexing",
	  "VAL is a comma separated list of file system types as found in your systems fstab, for example ext3,ext4,dosfs" },
	{ &opt_hide_file_names, "hide-file-names",  0 , DUCRC_TYPE_BOOL,   "hide file names in index (privacy)", 
	  "the names of directories will be preserved, but the names of the individual files will be hidden" },
	{ &opt_max_depth,       "max-depth",       'm', DUCRC_TYPE_INT,    "limit directory names to given depth" ,
	  "when this option is given duc will traverse the complete file system, but will only the first VAL "
	  "levels of directories in the database to reduce the size of the index" },
	{ &opt_one_file_system, "one-file-system", 'x', DUCRC_TYPE_BOOL,   "skip directories on different file systems" },
	{ &opt_progress,        "progress",        'p', DUCRC_TYPE_BOOL,   "show progress during indexing" },
	{ &opt_uncompressed,    "uncompressed",     0 , DUCRC_TYPE_BOOL,   "do not use compression for database",
          "Duc enables compression if the underlying database supports this. This reduces index size at the cost "
	  "of slightly longer indexing time" },
	{ NULL }
};


struct cmd cmd_index = {
	.name = "index",
	.descr_short = "Scan the filesystem and generate the Duc index",
	.usage = "[options] PATH ...",
	.init = index_init,
	.main = index_main,
	.options = options,
	.descr_long = 
		"The 'index' subcommand performs a recursive scan of the given paths on the\n"
		"filesystem and calculates the inclusive size of all directories. The results\n"
		"are written to the index, and can later be queried by one of the other duc tools.\n"
};

/*
 * End
 */

