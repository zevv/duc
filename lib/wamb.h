
#ifndef wamb_h
#define wamb_h

#include <fcntl.h>
#include <fcntl.h>



/*
 * Create and destroy wamb context
 */

enum {
	WAMB_OPEN_RO = 1<<0,
	WAMB_OPEN_RW = 1<<1,
};

struct wamb;

struct wamb *wamb_open(const char *path_db, int flags);
void wamb_close(struct wamb *wamb);


/*
 * Index file systems
 */


enum {
	WAMB_INDEX_XDEV	= 1<<0,
};

int wamb_index(struct wamb *wamb, const char *path, int flags);


/*
 * Reading the wamb database
 */

typedef struct wamb_iter wamb_iter;

struct wamb_ent {
	char name[256];
	off_t size;
	mode_t mode;
	dev_t dev;
	ino_t ino;
};

wamb_iter *wamb_list_path(struct wamb *wamb, const char *path);
void wamb_iter_read(wamb_iter *wi);
void wamb_iter_seek(wamb_iter *wi, long offset);
void wamb_iter_close(wamb_iter *wi);

#endif
