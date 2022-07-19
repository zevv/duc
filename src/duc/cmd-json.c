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
	

static bool opt_apparent = false;
static char *opt_database = NULL;
static double opt_min_size = 0;
static bool opt_exclude_files = false;


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
		switch(*s) {
			case '"': printf("\""); break;
			case '\t': putchar('\t'); break;
			case '\n': putchar('\n'); break;
			case '\r': putchar('\r'); break;
			default: putchar(*s); break;
		}
		s++;
	}
}


static void dump(duc *duc, duc_dir *dir, int depth, off_t min_size, int ex_files)
{
	struct duc_dirent *e;
	bool first = true;

	duc_size_type st = opt_apparent ? DUC_SIZE_TYPE_APPARENT : DUC_SIZE_TYPE_ACTUAL;

	while( (e = duc_dir_read(dir, DUC_SIZE_TYPE_ACTUAL, DUC_SORT_SIZE)) != NULL) {

		off_t size = duc_get_size(&e->size, st);

		if (!first) printf(",\n");

		if(e->type == DUC_FILE_TYPE_DIR && size>= min_size) {
			duc_dir *dir_child = duc_dir_openent(dir, e);
			if(dir_child) {
				indent(depth);
				printf("{\n");

				indent(depth + 2);
				printf("\"name\": \"");
				print_escaped(e->name); 
				printf("\",\n");

				indent(depth + 2);
				printf("\"count\": %jd,\n", e->size.count);

				indent(depth + 2);
				printf("\"size_apparent\": %jd,\n", e->size.apparent); 

				indent(depth + 2);
				printf("\"size_actual\": %jd,\n", e->size.actual);
				
				indent(depth + 2);
				printf("\"children\": [\n");
				
				dump(duc, dir_child, depth + 4, min_size, ex_files);

				printf("\n");
				indent(depth + 2);
				printf("]\n");
				
				indent(depth);
				printf("}");
			}
			duc_dir_close(dir_child);
		} else {
			if(!ex_files && size >= min_size) {
				indent(depth);
				printf("{\n");

				indent(depth + 2);
				printf("\"name\": \"");
				print_escaped(e->name); 
				printf("\",\n");

				indent(depth + 2);
				printf("\"size_apparent\": %jd,\n", e->size.apparent); 

				indent(depth + 2);
				printf("\"size_actual\": %jd\n", e->size.actual);
				
				indent(depth);
				printf("}");
			}
		}
		first = false;
	}
}


static int json_main(duc *duc, int argc, char **argv)
{
	char *path = ".";
	if(argc > 0) path = argv[0];

	int r = duc_open(duc, opt_database, DUC_OPEN_RO);
	if(r != DUC_OK) {
		duc_log(duc, DUC_LOG_FTL, "%s", duc_strerror(duc));
		return -1;
	}

	duc_dir *dir = duc_dir_open(duc, path);
	if(dir == NULL) {
		duc_log(duc, DUC_LOG_FTL, "%s", duc_strerror(duc));
		return -1;
	}

	struct duc_size size;
	duc_dir_get_size(dir, &size);

	printf("{\n");

	indent(2);
	printf("\"name\": \"");
	print_escaped(path); 
	printf("\",\n");

	indent(2);
	printf("\"count\": %jd,\n", size.count);

	indent(2);
	printf("\"size_apparent\": %jd,\n", size.apparent); 

	indent(2);
	printf("\"size_actual\": %jd,\n", size.actual);
	
	indent(2);
	printf("\"children\": [\n");
	
	dump(duc, dir, 4, (off_t)opt_min_size, opt_exclude_files);
	printf("\n");

	indent(2);
	printf("]\n");

	printf("}\n");

	duc_dir_close(dir);
	duc_close(duc);

	return 0;
}


static struct ducrc_option options[] = {
	{ &opt_apparent,      "apparent",      'a', DUCRC_TYPE_BOOL,   "interpret min_size/-s value as apparent size" },
	{ &opt_database,      "database",      'd', DUCRC_TYPE_STRING, "select database file to use [~/.duc.db]" },
	{ &opt_exclude_files, "exclude-files", 'x', DUCRC_TYPE_BOOL,   "exclude file from json output, only include directories" },
	{ &opt_min_size,      "min_size",      's', DUCRC_TYPE_DOUBLE, "specify min size for files or directories" },
	{ NULL }
};


struct cmd cmd_json = {
	.name = "json",
	.descr_short = "Dump JSON output",
	.usage = "[options] [PATH]",
	.main = json_main,
	.options = options,
};


/*
 * End
 */
