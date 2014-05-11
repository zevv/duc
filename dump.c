
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "db.h"


char *human_size(off_t size)
{
	char prefix[] = { '\0', 'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y' };
	double v = size;
	char *p = prefix;
	char *s = NULL;

	if(size < 1024) {
		int r = asprintf(&s, "%6jd", size);
		if(r != -1) return s;
	} else {
		while(v >= 1024.0) {
			v /= 1024.0;
			p ++;
		}
		int r = asprintf(&s, "%5.1f%c", v, *p);
		if(r != -1) return s;
	}

	return NULL;
}


int dump(struct db *db, const char *path)
{
	struct db_node *node;

	node = db_find_dir(db, path);

	if(node == NULL) return -1;

	size_t i;

	off_t size_total = 0;
	off_t size_max = 0;

	for(i=0; i<node->child_count; i++) {
		struct db_child *child = &node->child_list[i];
		if(child->size > size_max) size_max = child->size;
		size_total += child->size;
	}
	
	for(i=0; i<node->child_count; i++) {
		struct db_child *child = &node->child_list[i];

		int w = 50 * child->size / size_max;

		char *siz = human_size(child->size);
		printf("%-20.20s %s ", child->name, siz);
		free(siz);

		int j;
		for(j=0; j<w; j++) putchar('#');
		printf("\n");

		child ++;
	}
	
	return 0;
}


/*
 * End
 */

