
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

	printf("Size   File Count\n");
	while(( report = duc_get_report(duc, i)) != NULL) {

	    size_t count;
	    setlocale(LC_NUMERIC, "");
	    for (int i=0; i < DUC_HISTOGRAM_MAX; i++) {
		count = report->histogram[i];
		if (count != 0) {
		    printf("2^%-07d  %'d\n",i, count);
		}
	    }
			
	    duc_index_report_free(report);
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
	{ &opt_base,      "base10",    't', DUCRC_TYPE_BOOL,   "show histogram in base 10 bucket spacing, default base2 bucket sizes." },
	{ &opt_bytes,     "bytes",     'b', DUCRC_TYPE_BOOL,   "show bucket size in exact number of bytes" },
	{ &opt_database,  "database",  'd', DUCRC_TYPE_STRING, "select database file to use [~/.duc.db]" },
	{ NULL }
};


struct cmd cmd_histogram = {
	.name = "histogram",
	.descr_short = "Dump histogram info",
	.usage = "[options]",
	.main = histogram_main,
	.options = options,
};


/*
 * End
 */

