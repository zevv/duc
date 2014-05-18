
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


static int index_main(int argc, char **argv)
{
	int c;
	char *path_db = NULL;
	int index_flags = 0;
	int open_flags = DUC_OPEN_RW | DUC_OPEN_LOG_INF;

	struct option longopts[] = {
		{ "compress",        no_argument,       NULL, 'c' },
		{ "database",        required_argument, NULL, 'd' },
		{ "one-file-system", required_argument, NULL, 'x' },
		{ "quiet",           no_argument,       NULL, 'q' },
		{ "verbose",         required_argument, NULL, 'v' },
		{ NULL }
	};

	while( ( c = getopt_long(argc, argv, "cd:qxv", longopts, NULL)) != EOF) {

		switch(c) {
			case 'c':
				open_flags |= DUC_OPEN_COMPRESS;
				break;
			case 'd':
				path_db = optarg;
				break;
			case 'q':
				open_flags &= ~DUC_OPEN_LOG_INF;
				break;
			case 'x':
				index_flags |= DUC_INDEX_XDEV;
				break;
			case 'v':
				open_flags |= DUC_OPEN_LOG_DBG;
				break;
			default:
				return -2;
		}
	}
	
	argc -= optind;
	argv += optind;

	if(argc < 1) {
		fprintf(stderr, "Required index PATH missing.\n");
		return -2;
	}
	
	struct duc *duc = duc_new();
	if(duc == NULL) {
                fprintf(stderr, "Error creating duc context\n");
                return -1;
        }
	
	int r = duc_open(duc, path_db, open_flags);
	if(r != DUC_OK) {
		fprintf(stderr, "%s\n", duc_strerror(duc));
		return -1;
	}

	/* Index all paths passed on the cmdline */

	int i;
	for(i=0; i<argc; i++) {
		struct duc_index_report report;
		int r = duc_index(duc, argv[i], index_flags, &report);
		char siz[16];
		duc_humanize(report.size_total, siz, sizeof siz);
		if(r == DUC_OK) {
		  char human[120];
		  duc_fmttime(human, report.time_start, report.time_stop);
			fprintf(stderr, "Indexed %zu files and %zu directories, (%sB total) in %s\n", 
					report.file_count, 
					report.dir_count,
					siz,
					human);
		} else {
			fprintf(stderr, "An error occured while indexing: %s", duc_strerror(duc));
		}

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
		"  -c, --compress          create compressed database, favour size over speed\n"
		"  -d, --database=ARG      use database file ARG [~/.duc.db]\n"
		"  -q, --quiet             do not report errors\n"
		"  -x, --one-file-system   don't cross filesystem boundaries\n"
		"  -v, --verbose           show what is happening\n"
		,
	.main = index_main
};
/*
 * End
 */

