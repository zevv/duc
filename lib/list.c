
#include <stdlib.h>
#include <assert.h>

#include "wamb.h"
#include "db.h"

struct wamb_iter {

};


wamb_iter *wamb_list_path(const char *path_db)
{
	struct wamb_iter *wi;

	wi = malloc(sizeof *wi);
	assert(wi);

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
