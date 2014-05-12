

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>

#include "cmd.h"
#include "duc.h"


static int test_main(int argc, char **argv)
{
	duc *duc = duc_open(NULL, DUC_OPEN_RO);

	char *path = argv[1];

	ducdir *dir = duc_opendir(duc, path);
	printf("%p %s\n", dir, strerror(errno));

	if(dir) {
		struct ducent *e;
		while( (e = duc_readdir(dir)) != NULL) {
			printf("%s %jd\n", e->name, e->size);
		}

		duc_closedir(dir);
		duc_close(duc);
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

