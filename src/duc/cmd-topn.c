
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

	int i = 0;

	int r = duc_open(duc, file, DUC_OPEN_RO);
	if(r != DUC_OK) {
		duc_log(duc, DUC_LOG_FTL, "%s", duc_strerror(duc));
		return -1;
	}

	printf("%*s Filename\n",12,"Size");
	while(( report = duc_get_report(duc, i)) != NULL) {

	    size_t count;
	    // Get number of topN files actually found.
	    int topn_cnt = report->topn_cnt;

	    setlocale(LC_NUMERIC, "");
	    // Counting DOWN from largest to smallest, assumes array already sorted.
	    for (int i=topn_cnt-1; i >= 0; i--) {
		size_t size = report->topn_array[i]->size;
		if ( size != 0) {
		    char siz[32];
		    struct duc_size dsize;
		    dsize.actual = size;
		    duc_human_size(&dsize, st, opt_bytes, siz, sizeof siz);
		    printf("%*s", 12, siz);
		    printf(" %s\n", report->topn_array[i]->name);
		}
	    }
			
	    duc_index_report_free(report);
	    i++;
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

