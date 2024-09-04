
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
#include <math.h>

#include "cmd.h"
#include "duc.h"

static bool opt_apparent = false;
static bool opt_base = false;
static bool opt_bytes = false;
static char *opt_database = NULL;

static int histogram_db(duc *duc, char *file)
{
	struct duc_index_report *report;
	duc_size_type st = opt_apparent ? DUC_SIZE_TYPE_APPARENT : DUC_SIZE_TYPE_ACTUAL;
	int i = 0;

	int r = duc_open(duc, file, DUC_OPEN_RO);
	if(r != DUC_OK) {
		duc_log(duc, DUC_LOG_FTL, "%s", duc_strerror(duc));
		return -1;
	}

	while(( report = duc_get_report(duc, i)) != NULL) {

	    printf("Path: %s\n%3s %10s %10s\n",report->path,"Bkt","Size","Count");

	    size_t count;
	    size_t bucket_size = 0;
	    char pretty[32];
	    setlocale(LC_NUMERIC, "");
	    for (int i=0; i < report->histogram_buckets; i++) {
		count = report->histogram[i];
		bucket_size = pow(2, i);
		int ret = humanize(bucket_size, 0, 1024, pretty, sizeof pretty);
		printf("%3d %10s %'10d\n",i, pretty, count);
	    }
			
	    duc_index_report_free(report);
	    printf("\n");
	    i++;
	}

	duc_close(duc);
	
	return 0;
}


static int histogram_main(duc *duc, int argc, char **argv)
{
    return(histogram_db(duc, opt_database));
}


static struct ducrc_option options[] = {
	{ &opt_apparent,  "apparent",  'a', DUCRC_TYPE_BOOL,   "show apparent instead of actual file size" },
	{ &opt_bytes,     "bytes",     'b', DUCRC_TYPE_BOOL,   "show bucket size in exact number of bytes" },
	{ &opt_database,  "database",  'd', DUCRC_TYPE_STRING, "select database file to use [~/.duc.db]" },
	{ &opt_base,      "base10",    't', DUCRC_TYPE_BOOL,   "show histogram in base 10 bucket spacing, default base2 bucket sizes." },
	{ NULL }
};


struct cmd cmd_histogram = {
	.name = "histogram",
	.descr_short = "Dump histogram of file sizes found.",
	.usage = "[options]",
	.main = histogram_main,
	.options = options,
};


/*
 * End
 */

