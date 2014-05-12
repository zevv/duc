
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
	int one_file_system = 0;

	struct option longopts[] = {
		{ "database",        required_argument, NULL, 'd' },
		{ "one-file-system", required_argument, NULL, 'x' },
		{ NULL }
	};

	while( ( c = getopt_long(argc, argv, "d:x", longopts, NULL)) != EOF) {

		switch(c) {
			case 'd':
				path_db = optarg;
				break;
			case 'x':
				one_file_system = 1;
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

	int flags = 0;
	if(one_file_system) flags |= WAMB_INDEX_XDEV;

	/* Index all paths passed on the cmdline */

	int i;
	for(i=0; i<argc; i++) {
		fprintf(stderr, "Indexing %s\n", argv[i]);
		wamb_index(wamb, argv[i], flags);
	}

	wamb_close(wamb);

	return 0;
}



struct cmd cmd_index = {
	.name = "index",
	.description = "Index filesystem",
	.usage = "[options] PATH ...",
	.help = 
		"Valid options:\n"
		"\n"
		"  -d, --database=ARG     use database file ARG\n"
		"  -x, --one-file-system  don't cross filesystem boundaries\n"
		,
	.main = index_main
};
/*
 * End
 */

