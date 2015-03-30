
#ifndef duc_h
#define duc_h

#include <limits.h>
#include <stdio.h>
#include <stdarg.h>
#include <dirent.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/time.h>
#include <glob.h>

typedef struct duc duc;
typedef struct duc_dir duc_dir;
typedef struct duc_index_req duc_index_req;

typedef enum {
	DUC_OPEN_RO = 1<<0,        /* Open read-only (for querying)*/
	DUC_OPEN_RW = 1<<1,        /* Open read-write (for indexing) */
	DUC_OPEN_COMPRESS = 1<<2,  /* Create compressed database */
	DUC_OPEN_FORCE = 1<<3,     /* Force over-write of database for indexing */
} duc_open_flags;


typedef enum {
	DUC_INDEX_XDEV            = 1<<0, /* Do not cross device boundaries while indexing */
	DUC_INDEX_HIDE_FILE_NAMES = 1<<1, /* Hide file names */
} duc_index_flags;


typedef enum {
	DUC_OK,                     /* No error, success */
	DUC_E_DB_NOT_FOUND,         /* Database not found */
	DUC_E_DB_CORRUPT,           /* Database corrupt and unusable */
	DUC_E_DB_VERSION_MISMATCH,  /* Database version mismatch */
	DUC_E_PATH_NOT_FOUND,       /* Requested path not found */
	DUC_E_PERMISSION_DENIED,    /* Permission denied */
	DUC_E_OUT_OF_MEMORY,        /* Out of memory */
	DUC_E_DB_TCBDBNEW,          /* Unable to initialize TokyoCabinet DB */
	DUC_E_UNKNOWN,              /* Unknown error, contact the author */
} duc_errno;


typedef enum {
	DUC_LOG_FTL,
	DUC_LOG_WRN,
	DUC_LOG_INF,
	DUC_LOG_DBG,
	DUC_LOG_DMP
} duc_log_level;



struct duc_index_report {
	char path[PATH_MAX];        /* Indexed path */
	dev_t dev;                  /* Index top device id */
	ino_t ino;                  /* Index top inode */
	struct timeval time_start;  /* Index start time */
	struct timeval time_stop;   /* Index finished time */
	size_t file_count;          /* Total number of files indexed */
	size_t dir_count;           /* Total number of directories indexed */
	off_t size_total;           /* Total disk size indexed */
};

struct duc_dirent {
	char *name;                 /* File name */
	off_t size;                 /* File size */
	uint8_t type;               /* File type, one of POSIX's DT_* */
	dev_t dev;                  /* ID of device containing file */
	ino_t ino;                  /* inode number */
};


/*
 * Duc context, logging and error reporting
 */

typedef void (*duc_log_callback)(duc_log_level level, const char *fmt, va_list va);

duc *duc_new(void);
void duc_del(duc *duc);

void duc_set_log_level(duc *duc, duc_log_level level);
void duc_set_log_callback(duc *duc, duc_log_callback cb);

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
int duc_index_req_set_maxdepth(duc_index_req *req, int maxdepth);
struct duc_index_report *duc_index(duc_index_req *req, const char *path, duc_index_flags flags);
int duc_index_req_free(duc_index_req *req);
int duc_index_report_free(struct duc_index_report *rep);


/*
 * Querying the duc database
 */

struct duc_index_report *duc_get_report(duc *duc, size_t id);

duc_dir *duc_dir_open(duc *duc, const char *path);
duc_dir *duc_dir_get_parent(duc_dir *dir);
duc_dir *duc_dir_openat(duc_dir *dir, const char *name);
duc_dir *duc_dir_openent(duc_dir *dir, struct duc_dirent *e);
struct duc_dirent *duc_dir_read(duc_dir *dir);
char *duc_dir_get_path(duc_dir *dir);
off_t duc_dir_get_size(duc_dir *dir);
size_t duc_dir_get_count(duc_dir *dir);
struct duc_dirent *duc_dir_find_child(duc_dir *dir, const char *name);
int duc_dir_rewind(duc_dir *dir);
int duc_dir_close(duc_dir *dir);

/* 
 * Helper functions
 */

char *duc_human_size(off_t size);
char *duc_human_duration(struct timeval start, struct timeval end);
size_t duc_find_dbs(const char *db_dir_path, glob_t *db_list);
void duc_log(struct duc *duc, duc_log_level lvl, const char *fmt, ...);

#endif
