
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "cmd.h"
#include "duc.h"


static int info_main(int argc, char **argv)
{
	char *path_db = NULL;
	int c;
	
	struct option longopts[] = {
		{ "database",       required_argument, NULL, 'd' },
		{ NULL }
	};

	while( ( c = getopt_long(argc, argv, "d", longopts, NULL)) != EOF) {

		switch(c) {
			case 'd':
				path_db = optarg;
				break;
			default:
				return -2;
		}
	}

	
	duc *duc = duc_new();
	if(duc == NULL) {
                fprintf(stderr, "Error creating duc context\n");
                return -1;
        }

	int r = duc_open(duc, path_db, DUC_OPEN_RO);
	if(r != DUC_OK) {
		fprintf(stderr, "%s\n", duc_strerror(duc));
		return -1;
	}

	struct duc_index_report report;
	int i = 0;

	printf("Available indices:\n");
	while( duc_list(duc, i, &report) == 0) {

		char ts[32];
		struct tm *tm = localtime(&report.time_start);
		strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S",tm);

		char siz[32];
		duc_humanize(report.size_total, siz, sizeof siz);

		printf("  %s %7.7s %s\n", ts, siz, report.path);
		i++;
	}

	duc_close(duc);
	duc_del(duc);

	return 0;
}



struct cmd cmd_info = {
	.name = "info",
	.description = "Dump database info",
	.usage = "[options]",
	.help = 
		"  -d, --database=ARG      use database file ARG [~/.duc.db]\n",
	.main = info_main
};


/*
 * End
 */

