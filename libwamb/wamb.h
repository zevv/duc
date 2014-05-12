
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

typedef struct wamb wamb;

wamb *wamb_open(const char *path_db, int flags);
void wamb_close(wamb *wamb);


/*
 * Index file systems
 */


enum {
	WAMB_INDEX_XDEV	= 1<<0,
	WAMB_INDEX_VERBOSE = 1<<1,
	WAMB_INDEX_QUIET = 1<<2,
};

int wamb_index(wamb *wamb, const char *path, int flags);


/*
 * Reading the wamb database
 */

typedef struct wambdir wambdir;

struct wambent {
	char name[256];
	off_t size;
	mode_t mode;
	dev_t dev;
	ino_t ino;
};

wambdir *wamb_opendir(wamb *wamb, const char *path);
wambdir *wamb_openat(wamb *wamb, wambdir *dir, const char *name);
struct wambent *wamb_readdir(wambdir *dir);
struct wambent *wamb_finddir(wambdir *dir, const char *name);
int wamb_rewinddir(wambdir *dir);
void wamb_closedir(wambdir *dir);

#endif
