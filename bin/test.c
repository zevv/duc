

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>

#include "cmd.h"
#include "wamb.h"


static int test_main(int argc, char **argv)
{
	wamb *wamb = wamb_open(NULL, WAMB_OPEN_RO);

	char *path = argv[1];

	wambdir *dir = wamb_opendir(wamb, path);
	printf("%p %s\n", dir, strerror(errno));

	if(dir) {
		struct wambent *e;
		while( (e = wamb_readdir(dir)) != NULL) {
			printf("%s %jd\n", e->name, e->size);
		}

		wamb_closedir(dir);
		wamb_close(wamb);
	}

	return 0;
}



struct cmd cmd_test = {
	.name = "test",
	.description = "Test",
	.usage = "[options] PATH",
	.help = 
		"Valid options:\n"
		"\n"
		"  -d, --database=ARG     Use database file ARG\n"
		,
	.main = test_main
};
/*
 * End
 */

