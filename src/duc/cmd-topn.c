
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

static int topn_db(duc *duc, char *file)
{
	struct duc_index_report *report;
	duc_size_type st = DUC_SIZE_TYPE_ACTUAL;
	duc_sort sort = DUC_SORT_SIZE;

	// We can have multiple reports in a single DB... needs more testing.
	int rep_idx = 0;

	int r = duc_open(duc, file, DUC_OPEN_RO);
	if(r != DUC_OK) {
		duc_log(duc, DUC_LOG_FTL, "%s", duc_strerror(duc));
		return -1;
	}

	printf("%*s Filename\n",12,"Size");
	while(( report = duc_get_report(duc, rep_idx)) != NULL) {

	    size_t count;
	    // Get number of topN files actually stored in report.
	    int topn_cnt = report->topn_cnt;

	    setlocale(LC_NUMERIC, "");
	    // Counting DOWN from largest to smallest, assumes array already sorted.
	    for (int idx=topn_cnt-1; idx >= 0; idx--) {
		size_t size = report->topn_array[idx]->size;
		if ( size != 0) {
		    // FIXME - replace 32 with correct #define
		    char siz[32];
		    struct duc_size dsize;
		    dsize.actual = size;
		    duc_human_size(&dsize, st, opt_bytes, siz, sizeof siz);
		    // FIXME - replace 12 with correct #define
		    printf("%*s", 12, siz);
		    printf(" %s\n", report->topn_array[idx]->name);
		}
	    }
			
	    duc_index_report_free(report);
	    rep_idx++;
	}

	duc_close(duc);
	
	return 0;
}


static int topn_main(duc *duc, int argc, char **argv)
{
    return(topn_db(duc, opt_database));
}


static struct ducrc_option options[] = {
	{ &opt_bytes,     "bytes",     'b', DUCRC_TYPE_BOOL,   "show file size in exact number of bytes" },
	{ &opt_database,  "database",  'd', DUCRC_TYPE_STRING, "select database file to use [~/.duc.db]" },
	{ NULL }
};


struct cmd cmd_topn = {
	.name = "topn",
	.descr_short = "Dump N largest files in DB.",
	.usage = "[options]",
	.main = topn_main,
	.options = options,
};


/*
 * End
 */

