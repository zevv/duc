
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>



#ifdef NEE


void dump_node(struct node *n, int depth)
{
	int i;

	for(i=0; i<depth; i++) printf(" ");
	printf("%s %d\n", n->name, s->size);

	struct node *nc = node->child_list; 
	while(nc) {
		nc = nc->next;
	}
}

#endif

int dump(const char *path)
{
#ifdef NEE
	char *path_canon = realpath(path, NULL);
	if(path_canon == NULL) {
		fprintf(stderr, "Error converting path %s: %s\n", path, strerror(errno));
		return 0;
	}

	struct node *n;

	db = db_open("/tmp/db");

	n = db_find_node(db, path_canon);

	dump_node(db, n, 0);

	free(path_canon);

#endif
	return 0;
}


/*
 * End
 */

