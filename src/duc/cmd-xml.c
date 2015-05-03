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
	

static char *opt_database = NULL;
static double opt_min_size = 0;
static int opt_exclude_files = 0;


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


static void dump(duc *duc, duc_dir *dir, int depth, off_t min_size, int ex_files)
{
	struct duc_dirent *e;

	while( (e = duc_dir_read(dir, DUC_SIZE_TYPE_ACTUAL)) != NULL) {

		if(e->type == DT_DIR && e->size.apparent >= min_size) {
			indent(depth);
			printf("<ent type='dir' name='");
			print_escaped(e->name);
			printf("' size_apparent='%jd' size_actual='%jd'>\n", (intmax_t)e->size.apparent, (intmax_t)e->size.actual);
			duc_dir *dir_child = duc_dir_openent(dir, e);
			if(dir_child) {
				dump(duc, dir_child, depth + 1, min_size, ex_files);
				indent(depth);
				printf("</ent>\n");
			}
		} else {
			if(!ex_files && e->size.apparent >= min_size) {
				indent(depth);
				printf("<ent name='");
				print_escaped(e->name);
				printf("' size_apparent='%jd' size_actual='%jd' />\n", (intmax_t)e->size.apparent, (intmax_t)e->size.actual);
			}
		}
	}
}


static int xml_main(duc *duc, int argc, char **argv)
{
	char *path = ".";
	if(argc > 0) path = argv[0];

	int r = duc_open(duc, opt_database, DUC_OPEN_RO);
	if(r != DUC_OK) {
		return -1;
	}

	duc_dir *dir = duc_dir_open(duc, path);
	if(dir == NULL) {
		duc_log(duc, DUC_LOG_WRN, "%s", duc_strerror(duc));
		return -1;
	}

	struct duc_size size;
	duc_dir_get_size(dir, &size);
	printf("<?xml version='1.0' encoding='UTF-8'?>\n");
	printf("<duc root='%s' size_apparent='%jd' size_actual='%jd'>\n", 
			path, (uintmax_t)size.apparent, (uintmax_t)size.actual);

	dump(duc, dir, 1, opt_min_size, opt_exclude_files);

	printf("</duc>\n");

	duc_dir_close(dir);
	duc_close(duc);

	return 0;
}


static struct ducrc_option options[] = {
	{ &opt_database,      "database",      'd', DUCRC_TYPE_STRING, "select database file to use [~/.duc.db]" },
	{ &opt_exclude_files, "exclude-files", 'x', DUCRC_TYPE_BOOL,   "exclude file from xml output, only include directories" },
	{ &opt_min_size,      "min_size",      's', DUCRC_TYPE_DOUBLE, "specify min size for files or directories" },
	{ NULL }
};


struct cmd cmd_xml = {
	.name = "xml",
	.descr_short = "Dump XML output",
	.usage = "[options] [PATH]",
	.main = xml_main,
	.options = options,
};


/*
 * End
 */
