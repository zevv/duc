#include "config.h"

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

#include "cmd.h"
#include "duc.h"

static void indent(int n)
{
	int i;
	for(i=0; i<n; i++) {
		putchar(' ');
	}
}

static void print_escaped(const char *s)
{
	while(*s) {
		if(isprint(*s)) {
			if(*s == '&') {
				fwrite("&amp;", 1, 5, stdout);
			} else if(*s == '\'') {
				fwrite("&apos;", 1, 6, stdout);
			} else {
				putchar(*s);
			}
		}
		s++;
	}
}

static void dump(duc *duc, duc_dir *dir, int depth, int min_size, int ex_files)
{
	struct duc_dirent *e;

	while( (e = duc_dir_read(dir)) != NULL) {

		if(e->type == DT_DIR && e->size >= min_size) {
			indent(depth);
			printf("<ent type='dir' name='");
			print_escaped(e->name);
			printf("' size='%ld'>\n", (long)e->size);
			duc_dir *dir_child = duc_dir_openent(dir, e);
			if(dir_child) {
				dump(duc, dir_child, depth + 1, min_size, ex_files);
				indent(depth);
				printf("</ent>\n");
			}
		} else {
			if(!ex_files && e->size >= min_size) {
				indent(depth);
				printf("<ent name='");
				print_escaped(e->name);
				printf("' size='%ld' />\n", (long)e->size);
			}
		}
	}
}


static int xml_main(int argc, char **argv)
{
	int c;
	int min_size = 0;
	int ex_files = 0;
	char *path_db = NULL;
	duc_log_level loglevel = DUC_LOG_WRN;

	struct option longopts[] = {
		{ "database",      required_argument, NULL, 'd' },
		{ "min-size",      required_argument, NULL, 's' },
		{ "exclude-files", no_argument,       NULL, 'x' },
		{ NULL }
	};

	while( ( c = getopt_long(argc, argv, "ds:xqv", longopts, NULL)) != EOF) {

		switch(c) {
			case 'd':
				path_db = optarg;
				break;
			case 's':
				min_size = atoi(optarg);
				break;
			case 'x':
				ex_files = 1;
				break;
			case 'q':
				loglevel = DUC_LOG_FTL;
				break;
			case 'v':
				if(loglevel < DUC_LOG_DMP) loglevel ++;
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
	duc_set_log_level(duc, loglevel);

	int r = duc_open(duc, path_db, DUC_OPEN_RO);
	if(r != DUC_OK) {
		fprintf(stderr, "%s\n", duc_strerror(duc));
		return -1;
	}

	duc_dir *dir = duc_dir_open(duc, path);
	if(dir == NULL) {
		fprintf(stderr, "%s\n", duc_strerror(duc));
		return -1;
	}

	printf("<?xml version='1.0' encoding='UTF-8'?>\n");
	printf("<duc root='%s' size='%ld'>\n", path, (long)duc_dir_get_size(dir));

	dump(duc, dir, 1, min_size, ex_files);

	printf("</duc>\n");

	duc_dir_close(dir);
	duc_close(duc);
	duc_del(duc);

	return 0;
}



struct cmd cmd_xml = {
	.name = "xml",
	.description = "Dump XML output",
	.usage = "[options] [PATH]",
	.help =
		"  -d, --database=ARG      	use database file ARG [~/.duc.db]\n"
		"  -s, --min-size VAL      	specify min size for files or directories\n"
		"  -x, --exclude-files		exclude file from xml output\n"
		"  -q, --quiet             	quiet mode, do not print any warnings\n",
	.main = xml_main
};


/*
 * End
 */
