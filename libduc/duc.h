
#ifndef duc_h
#define duc_h

#include <limits.h>

typedef struct duc duc;
typedef struct duc_dir duc_dir;
typedef struct duc_index_req duc_index_req;

typedef enum {
	DUC_OPEN_RO = 1<<0,        /* Open read-only (for querying)*/
	DUC_OPEN_RW = 1<<1,        /* Open read-write (for indexing) */
	DUC_OPEN_COMPRESS = 1<<2,  /* Create compressed database */
	DUC_OPEN_LOG_WRN = 1<<3,   /* Log warning messages */
	DUC_OPEN_LOG_INF = 1<<4,   /* Log informational messages */
	DUC_OPEN_LOG_DBG = 1<<5,   /* Log debug messages */
} duc_open_flags;


typedef enum {
	DUC_INDEX_XDEV	= 1<<0,    /* Do not cross device boundaries while indexing */
} duc_index_flags;


typedef enum {
	DUC_OK,                     /* No error, success */
	DUC_E_DB_NOT_FOUND,         /* Database not found */
	DUC_E_DB_VERSION_MISMATCH,  /* Database version mismatch */
	DUC_E_PATH_NOT_FOUND,       /* Requested path not found */
	DUC_E_PERMISSION_DENIED,    /* Permission denied */
	DUC_E_OUT_OF_MEMORY,        /* Out of memory */
	DUC_E_UNKNOWN,              /* Unknown error, contact the author */
} duc_errno;


typedef enum {
	DUC_MODE_REG,               /* Regular file */
	DUC_MODE_DIR,               /* Directory */
	DUC_MODE_CHR,               /* Character device */
	DUC_MODE_BLK,               /* Block device */
	DUC_MODE_FIFO,              /* Fifo */
	DUC_MODE_LNK,               /* Soft link */
	DUC_MODE_SOCK,              /* Unix socket */
	DUC_MODE_REST               /* Grouped 'rest', added by duc_limitdir() */
} duc_dirent_mode;


struct duc_index_report {
	char path[PATH_MAX];        /* Indexed path */
	dev_t dev;                  /* Index top device id */
	ino_t ino;                  /* Index top inode */
	time_t time_start;          /* Index start time */
	time_t time_stop;           /* Index finished time */
	size_t file_count;          /* Total number of files indexed */
	size_t dir_count;           /* Total number of directories indexed */
	off_t size_total;           /* Total disk size indexed */
};

struct duc_dirent {
	char name[256];             /* File name */
	off_t size;                 /* File size */
	duc_dirent_mode mode;       /* File mode */
	dev_t dev;                  /* ID of device containing file */
	ino_t ino;                  /* inode number */
};


/*
 * Create and destroy duc context
 */

duc *duc_new(void);
void duc_del(duc *duc);


/*
 * Error reporting
 */

duc_errno duc_error(duc *duc);
const char *duc_strerror(duc *duc);


/*
 * Open and close database
 */

int duc_open(duc *duc, const char *path_db, duc_open_flags flags);
int duc_close(duc *duc);


/*
 * Index file systems
 */

duc_index_req *duc_index_req_new(duc *duc);
int duc_index_req_add_exclude(duc_index_req *req, const char *pattern);
struct duc_index_report *duc_index(duc_index_req *req, const char *path, duc_index_flags flags);
int duc_index_req_free(duc_index_req *req);
int duc_index_report_free(struct duc_index_report *rep);


/*
 * Querying the duc database
 */

struct duc_index_report *duc_get_report(duc *duc, size_t id);

duc_dir *duc_opendir(duc *duc, const char *path);
duc_dir *duc_opendirat(duc *duc, struct duc_dirent *e);
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

/* 
 * Helper functions
 */

void duc_humanize(off_t size, char *buf, size_t buflen);

#endif
