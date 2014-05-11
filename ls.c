
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "ps.h"
#include "db.h"


static int human_readable = 0;
static int limit = 0;


char *human_size(off_t size)
{
	char prefix[] = { '\0', 'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y' };
	double v = size;
	char *p = prefix;
	char *s = NULL;

	if(!human_readable || size < 1024) {
		int r = asprintf(&s, "%10jd", size);
		if(r != -1) return s;
	} else {
		while(v >= 1024.0) {
			v /= 1024.0;
			p ++;
		}
		int r = asprintf(&s, "%9.1f%c", v, *p);
		if(r != -1) return s;
	}

	return NULL;
}


static int fn_comp_child(const void *a, const void *b)
{
	const struct db_child *da = a;
	const struct db_child *db = b;
	return(da->size < db->size);
}



static int ls(struct db *db, const char *path)
{
	struct db_node *node;
	int width = 80;

	/* Get terminal width, if a tty */

	if(isatty(0)) {
		struct winsize w;
		int r = ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
		if(r == 0) width = w.ws_col;
	}

	width = width - 36;

	char *path_canon = realpath(path, NULL);
	if(path_canon == NULL) {
		fprintf(stderr, "Error converting path %s: %s\n", path, strerror(errno));
		return -1;
	}

	node = db_find_dir(db, path_canon);
	free(path_canon);

	if(node == NULL) {
		fprintf(stderr, "Path not found in database\n");
		return -1;
	}
	qsort(node->child_list, node->child_count, sizeof(struct db_child), fn_comp_child);

	if(node == NULL) return -1;

	size_t i;

	off_t size_total = 0;
	off_t size_max = 0;

	for(i=0; i<node->child_count; i++) {
		struct db_child *child = &node->child_list[i];
		if(child->size > size_max) size_max = child->size;
		size_total += child->size;
	}

	off_t size_rest = 0;

	for(i=0; i<node->child_count; i++) {
		struct db_child *child = &node->child_list[i];

		if(limit == 0 || i < limit) {

			int w = width * child->size / size_max;

			char *siz = human_size(child->size);
			printf("%-20.20s %s ", child->name, siz);
			free(siz);

			int j;
			for(j=0; j<w; j++) putchar('#');
			printf("\n");
		} else {
			size_rest += child->size;
		}

		child ++;
	}

	if(size_rest > 0) {
		char *siz = human_size(size_rest);
		printf("%-20.20s %s\n", "Omitted files", siz);
		free(siz);
	}

	char *siz = human_size(size_total);
	printf("Total: %s\n", siz);
	free(siz);

	return 0;
}


static int ls_main(int argc, char **argv)
{
	int c;
	char *path_db = NULL;

	struct option longopts[] = {
		{ "database",       required_argument, NULL, 'd' },
		{ "human-readable", no_argument,       NULL, 'h' },
		{ "limit",          required_argument, NULL, 'n' },
		{ NULL }
	};

	while( ( c = getopt_long(argc, argv, "d:hn:", longopts, NULL)) != EOF) {

		switch(c) {
			case 'd':
				path_db = optarg;
				break;
			case 'h':
				human_readable = 1;
				break;
			case 'n':
				limit = atoi(optarg);
				break;
			default:
				return(-1);
		}
	}

	argc -= optind;
	argv += optind;

	char *path = ".";
	if(argc > 0) path = argv[0];
	struct db *db = db_open(path_db, "r");
	ls(db, path);
	db_close(db);

	return 0;
}



struct cmd cmd_ls = {
	.name = "ls",
	.description = "List directory",
	.usage = "[options] [PATH]",
	.help = 
		"Valid options:\n"
		"\n"
		"  -d, --database=ARG      use database file ARG\n"
		"  -h, --human-readable    print sizes in human readable format\n"
		"  -n, --limit=ARG         limit number of results\n",
	.main = ls_main
};


/*
 * End
 */

