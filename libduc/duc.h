
#ifndef duc_h
#define duc_h

/*
 * Create and destroy duc context
 */

enum {
	DUC_OPEN_RO = 1<<0,
	DUC_OPEN_RW = 1<<1,
	DUC_OPEN_COMPRESS = 1<<2,
	DUC_OPEN_LOG_WRN = 1<<3,
	DUC_OPEN_LOG_INF = 1<<4,
	DUC_OPEN_LOG_DBG = 1<<5,
};

enum {
	DUC_INDEX_XDEV	= 1<<0,
};

enum duc_errno {
	DUC_OK,                     /* No error, success */
	DUC_E_DB_NOT_FOUND,         /* Database not found */
	DUC_E_DB_VERSION_MISMATCH,  /* Database version mismatch */
	DUC_E_PATH_NOT_FOUND,       /* Requested path not found */
	DUC_E_PERMISSION_DENIED,    /* Permission denied */
	DUC_E_OUT_OF_MEMORY,        /* Out of memory */
	DUC_E_UNKNOWN,              /* Unknown error, contact the author */
};

typedef struct duc duc;
typedef enum duc_errno duc_errno;

duc *duc_open(const char *path_db, int flags, duc_errno *e);
void duc_close(duc *duc);

enum duc_errno duc_error(duc *duc);
const char *duc_strerror(enum duc_errno e);


/*
 * Index file systems
 */


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
off_t duc_sizedir(ducdir *dir);
struct ducent *duc_finddir(ducdir *dir, const char *name);
int duc_rewinddir(ducdir *dir);
int duc_closedir(ducdir *dir);

#endif
