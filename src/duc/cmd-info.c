
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

static bool opt_apparent = false;
static bool opt_bytes = false;
static char *opt_database = NULL;
static bool opt_histogram = false;

static int info_db(duc *duc, char *file)
{
	struct duc_index_report *report;
	duc_size_type st = opt_apparent ? DUC_SIZE_TYPE_APPARENT : DUC_SIZE_TYPE_ACTUAL;
	int i = 0;

	int r = duc_open(duc, file, DUC_OPEN_RO);
	if(r != DUC_OK) {
		duc_log(duc, DUC_LOG_FTL, "%s", duc_strerror(duc));
		return -1;
	}

	printf("Date       Time       Files    Dirs    Size Path\n");
	while(( report = duc_get_report(duc, i)) != NULL) {

		char ts[32];
		time_t t = report->time_start.tv_sec;
		struct tm *tm = localtime(&t);
		strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S",tm);

		char siz[32], fs[32], ds[32];
		duc_human_size(&report->size, st, opt_bytes, siz, sizeof siz);
		duc_human_number(report->file_count, opt_bytes, fs, sizeof fs);
		duc_human_number(report->dir_count, opt_bytes, ds, sizeof ds);

		printf("%s %7s %7s %7s %s\n", ts, fs, ds, siz, report->path);

		if (opt_histogram) {
		    size_t count;
		    printf("\nHistogram:\n----------\n");
		    for (int i=0; i < DUC_HISTOGRAM_MAX; i++) {
			count = report->histogram[i];
			if (count != 0) {
			    printf("2^%-02d %d\n",i, count);
			}
		    }
		}
		
		duc_index_report_free(report);
		i++;
	}

	duc_close(duc);

	return 0;
}


static int info_main(duc *duc, int argc, char **argv)
{
    return(info_db(duc, opt_database));
}


static struct ducrc_option options[] = {
	{ &opt_apparent,  "apparent",  'a', DUCRC_TYPE_BOOL,   "show apparent instead of actual file size" },
	{ &opt_bytes,     "bytes",     'b', DUCRC_TYPE_BOOL,   "show file size in exact number of bytes" },
	{ &opt_database,  "database",  'd', DUCRC_TYPE_STRING, "select database file to use [~/.duc.db]" },
	{ &opt_histogram, "histogram",'H', DUCRC_TYPE_BOOL,   "show file size in exact number of bytes" },
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

