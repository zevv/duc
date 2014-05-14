
#ifndef duc_h
#define duc_h

#include <limits.h>


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

struct duc_index_report {
	char path[PATH_MAX];
	dev_t dev;
	ino_t ino;
	time_t time_start;
	time_t time_stop;
	size_t file_count;
	size_t dir_count;
	off_t size_total;
};

int duc_index(duc *duc, const char *path, int flags, struct duc_index_report *report);


/*
 * Reading the duc database
 */

typedef struct duc_dir duc_dir;

struct duc_dirent {
	char name[256];
	off_t size;
	mode_t mode;
	dev_t dev;
	ino_t ino;
};

duc_dir *duc_opendir(duc *duc, const char *path);
duc_dir *duc_opendirat(duc *duc, dev_t dev, ino_t ino);
int duc_limitdir(duc_dir *dir, size_t count);
struct duc_dirent *duc_readdir(duc_dir *dir);
off_t duc_sizedir(duc_dir *dir);
struct duc_dirent *duc_finddir(duc_dir *dir, const char *name);
int duc_rewinddir(duc_dir *dir);
int duc_closedir(duc_dir *dir);


/* 
 * Graph drawing
 */

int duc_graph(duc_dir *dir, int size, int depth, FILE *fout);
int duc_graph_xy_to_path(duc_dir *dir, int size, int depth, int x, int y, char *path, size_t path_len);


#endif
