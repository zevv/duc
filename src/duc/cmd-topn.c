
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
	    int topn_cnt = report->topn_cnt;
	    int topn_max_cnt = report->topn_max_cnt;

	    printf(" Got path=%s, topn_cnt=%d, topn_max_cnt=%d\n",report->path,topn_cnt, topn_max_cnt);
	    setlocale(LC_NUMERIC, "");
	    // Counting DOWN from largest to smallest...
	    for (int i=topn_max_cnt; i > 0; i--) {
		size_t size = report->topn_array[i]->size;
		if ( size != 0) {
		    printf("2^%-07d  %'d\n",i, size);
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

