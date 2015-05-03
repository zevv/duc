
#include "config.h"

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cmd.h"
#include "duc.h"
	
static int opt_bytes = 0;
static char *opt_database = NULL;

static int info_db(duc *duc, char *file) 
{

	struct duc_index_report *report;
	int i = 0;

	int r = duc_open(duc, file, DUC_OPEN_RO);
	if(r != DUC_OK) {
		return -1;
	}

	while(( report = duc_get_report(duc, i)) != NULL) {

		char ts[32];
		struct tm *tm = localtime(&report->time_start.tv_sec);
		strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S",tm);

		char *siz = duc_human_size(&report->size, DUC_SIZE_TYPE_ACTUAL, opt_bytes);
		char *s = duc_human_duration(report->time_start, report->time_stop);
		printf("  %s %7.7s %s\n", ts, siz, report->path);
		printf("  Indexed %zu files and %zu directories in %s\n", report->file_count, report->dir_count, s);
		free(s);
		free(siz);

		duc_index_report_free(report);
		i++;
	}

	duc_close(duc);

	return 0;
}


static int info_main(duc *duc, int argc, char **argv)
{
	char *db_dir = NULL;

	if (db_dir) {
		glob_t bunch_of_dbs;
		char **db_file;
		size_t n = duc_find_dbs(db_dir, &bunch_of_dbs);
		printf("Found %zu (%zu) DBs to look at.\n", n, bunch_of_dbs.gl_pathc);
		int i = 0;
		for (db_file = bunch_of_dbs.gl_pathv; i < n; db_file++, i++) {
		info_db(duc, *db_file);
	    }
	    return 0;
	}

	info_db(duc, opt_database);
	return 0;
}


static struct ducrc_option options[] = {
	{ &opt_bytes,     "bytes",     'b', DUCRC_TYPE_BOOL,   "show file size in exact number of bytes" },
	{ &opt_database,  "database",  'd', DUCRC_TYPE_STRING, "select database file to use [~/.duc.db]" },
	{ NULL }
};


struct cmd cmd_info = {
	.name = "info",
	.descr_short = "Dump database info",
	.usage = "[options]",
	.main = info_main,
	.options = options,
};


/*
 * End
 */

