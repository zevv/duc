
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "duc.h"
#include "db.h"

struct duc_iter {
	struct duc *duc;

};


duc_iter *duc_list_path(struct duc *duc, const char *path)
{
	struct duc_iter *wi;
	
	wi = malloc(sizeof *wi);
	if(wi == NULL) return NULL;

	char *path_canon = realpath(path, NULL);
	if(path_canon == NULL) {
		fprintf(stderr, "Error converting path %s: %s\n", path, strerror(errno));
		free(wi);
		return NULL;
	}

	wi->duc = duc;

	return wi;
}


void duc_iter_read(duc_iter *wi)
{

}


void duc_iter_seek(duc_iter *wi, long offset)
{

}


void duc_iter_close(duc_iter *wi)
{

}


/*
 * End
 */
