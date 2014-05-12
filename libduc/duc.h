
#ifndef duc_h
#define duc_h

#include <fcntl.h>
#include <fcntl.h>



/*
 * Create and destroy duc context
 */

enum {
	WAMB_OPEN_RO = 1<<0,
	WAMB_OPEN_RW = 1<<1,
};

typedef struct duc duc;

duc *duc_open(const char *path_db, int flags);
void duc_close(duc *duc);


/*
 * Index file systems
 */


enum {
	WAMB_INDEX_XDEV	= 1<<0,
	WAMB_INDEX_VERBOSE = 1<<1,
	WAMB_INDEX_QUIET = 1<<2,
};

int duc_index(duc *duc, const char *path, int flags);


/*
 * Reading the duc database
 */

typedef struct ducdir ducdir;

struct ducent {
	char name[256];
	off_t size;
	mode_t mode;
	dev_t dev;
	ino_t ino;
};

ducdir *duc_opendir(duc *duc, const char *path);
ducdir *duc_opendirat(struct duc *duc, dev_t dev, ino_t ino);
struct ducent *duc_readdir(ducdir *dir);
struct ducent *duc_finddir(ducdir *dir, const char *name);
int duc_rewinddir(ducdir *dir);
void duc_closedir(ducdir *dir);

#endif
