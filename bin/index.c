
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
#include "wamb.h"


static int index_main(int argc, char **argv)
{
	int c;
	char *path_db = NULL;

	struct option longopts[] = {
		{ "database",       required_argument, NULL, 'd' },
		{ NULL }
	};

	while( ( c = getopt_long(argc, argv, "d:h", longopts, NULL)) != EOF) {

		switch(c) {
			case 'd':
				path_db = optarg;
				break;
			default:
				return(-1);
		}
	}
	
	argc -= optind;
	argv += optind;

	if(argc < 1) {
		fprintf(stderr, "Required index path missing.\n");
		return -1;
	}

	struct wamb *wamb = wamb_open(path_db, WAMB_OPEN_RW);
	if(wamb == NULL) return -1;

	wamb_index(wamb, argv[0], 0);
	wamb_close(wamb);

	return 0;
}



struct cmd cmd_index = {
	.name = "index",
	.description = "Index filesystem",
	.usage = "[options] PATH",
	.help = 
		"Valid options:\n"
		"\n"
		"  -d, --database=ARG     Use database file ARG\n"
		,
	.main = index_main
};
/*
 * End
 */

