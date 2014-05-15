
#include "config.h"

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "cmd.h"
#include "duc.h"

static void indent(int n)
{
	int i;
	for(i=0; i<n; i++) {
		putchar(' ');
	}
}

static void dump(duc *duc, duc_dir *dir, int depth)
{
	struct duc_dirent *e;

	while( (e = duc_readdir(dir)) != NULL) {

		if(e->mode == DUC_MODE_DIR) {
			indent(depth);
			printf("<ent name='%s' size='%jd'>\n", e->name, e->size);
			duc_dir *dir_child = duc_opendirat(duc, e);
			if(dir_child) {
				dump(duc, dir_child, depth + 1);
				indent(depth);
				printf("</ent>\n");
			}
		} else {
			indent(depth);
			printf("<ent name='%s' size='%jd' />\n", e->name, e->size);
		}
	}
}


static int xml_main(int argc, char **argv)
{
	int c;
	char *path_db = NULL;

	struct option longopts[] = {
		{ "database",       required_argument, NULL, 'd' },
		{ NULL }
	};

	while( ( c = getopt_long(argc, argv, "d:", longopts, NULL)) != EOF) {

		switch(c) {
			case 'd':
				path_db = optarg;
				break;
			default:
				return -2;
		}
	}

	argc -= optind;
	argv += optind;
	
	char *path = ".";
	if(argc > 0) path = argv[0];
	
	/* Open duc context */
	
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

	duc_dir *dir = duc_opendir(duc, path);
	if(dir == NULL) {
		fprintf(stderr, "%s\n", duc_strerror(duc));
		return -1;
	}

	printf("<?xml version='1.0' encoding='UTF-8'?>\n");
	printf("<duc root='%s' size='%jd'>\n", path, duc_sizedir(dir));
	dump(duc, dir, 1);
	printf("</duc>\n");

	duc_closedir(dir);
	duc_close(duc);
	duc_del(duc);

	return 0;
}



struct cmd cmd_xml = {
	.name = "xml",
	.description = "Dump XML output",
	.usage = "[options] [PATH]",
	.help = 
		"  -d, --database=ARG      use database file ARG [~/.duc.db]\n",
	.main = xml_main
};


/*
 * End
 */

