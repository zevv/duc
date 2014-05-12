
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "wamb.h"
#include "db.h"

struct wamb_iter {
	struct wamb *wamb;

};


wamb_iter *wamb_list_path(struct wamb *wamb, const char *path)
{
	struct wamb_iter *wi;
	
	wi = malloc(sizeof *wi);
	if(wi == NULL) return NULL;

	char *path_canon = realpath(path, NULL);
	if(path_canon == NULL) {
		fprintf(stderr, "Error converting path %s: %s\n", path, strerror(errno));
		free(wi);
		return NULL;
	}

	wi->wamb = wamb;

	return wi;
}


void wamb_iter_read(wamb_iter *wi)
{

}


void wamb_iter_seek(wamb_iter *wi, long offset)
{

}


void wamb_iter_close(wamb_iter *wi)
{

}


/*
 * End
 */
