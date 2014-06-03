
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

static struct option longopts[] = {
	{ "compress",        no_argument,       NULL, 'c' },
	{ "database",        required_argument, NULL, 'd' },
	{ "one-file-system", required_argument, NULL, 'x' },
	{ "quiet",           no_argument,       NULL, 'q' },
	{ "verbose",         required_argument, NULL, 'v' },
	{ NULL }
};


static int index_main(int argc, char **argv)
{
	int c;
	char *path_db = NULL;
	duc_index_flags index_flags = 0;
	int open_flags = DUC_OPEN_RW | DUC_OPEN_COMPRESS;
	duc_log_level loglevel = DUC_LOG_WRN;
	
	struct duc *duc = duc_new();
	if(duc == NULL) {
                fprintf(stderr, "Error creating duc context\n");
                return -1;
        }
		
	duc_index_req *req = duc_index_req_new(duc);

	while( ( c = getopt_long(argc, argv, "d:e:qxuv", longopts, NULL)) != EOF) {

		switch(c) {
			case 'd':
				path_db = optarg;
				break;
			case 'e':
				duc_index_req_add_exclude(req, optarg);
				break;
			case 'q':
				loglevel = DUC_LOG_FTL;
				break;
			case 'u':
				open_flags &= ~DUC_OPEN_COMPRESS;
				break;
			case 'v':
				if(loglevel < DUC_LOG_DMP) loglevel ++;
				break;
			case 'x':
				index_flags |= DUC_INDEX_XDEV;
				break;
			default:
				return -2;
		}
	}
				
	duc_set_log_level(duc, loglevel);

	argc -= optind;
	argv += optind;

	if(argc < 1) {
		fprintf(stderr, "Required index PATH missing.\n");
		return -2;
	}
	
	int r = duc_open(duc, path_db, open_flags);
	if(r != DUC_OK) {
		fprintf(stderr, "%s\n", duc_strerror(duc));
		return -1;
	}

	/* Index all paths passed on the cmdline */

	int i;
	for(i=0; i<argc; i++) {

		struct duc_index_report *report;
		report = duc_index(req, argv[i], index_flags);
		if(report == NULL) {
			fprintf(stderr, "%s\n", duc_strerror(duc));
			continue;
		}

		char *siz = duc_human_size(report->size_total);
		if(r == DUC_OK) {
			char *s = duc_human_duration(report->time_start, report->time_stop);
			fprintf(stderr, "Indexed %lu files and %lu directories, (%sB total) in %s\n", 
					(unsigned long)report->file_count, 
					(unsigned long)report->dir_count,
					siz,
					s);
			free(s);
		} else {
			fprintf(stderr, "An error occured while indexing: %s", duc_strerror(duc));
		}
		free(siz);

		duc_index_report_free(report);
	}

	duc_close(duc);
	duc_del(duc);

	return 0;
}



struct cmd cmd_index = {
	.name = "index",
	.description = "Index filesystem",
	.usage = "[options] PATH ...",
	.help = 
		"  -d, --database=ARG      use database file ARG [~/.duc.db]\n"
		"  -e, --exclude=PATTERN   exclude files matching PATTERN\n"
		"  -q, --quiet             do not report errors\n"
		"  -u, --uncompressed      do not use compression for database\n"
		"  -v, --verbose           verbose mode, can be passed two times for debugging\n"
		"  -x, --one-file-system   don't cross filesystem boundaries\n"
		,
	.main = index_main
};
/*
 * End
 */

