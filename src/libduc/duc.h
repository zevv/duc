
#ifndef duc_h
#define duc_h

#include <limits.h>
#include <stdio.h>
#include <stdarg.h>
#include <dirent.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/time.h>

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
	DUC_INDEX_XDEV             = 1<<0, /* Do not cross device boundaries while indexing */
	DUC_INDEX_HIDE_FILE_NAMES  = 1<<1, /* Hide file names */
	DUC_INDEX_CHECK_HARD_LINKS = 1<<2, /* Count hard links only once during indexing */
} duc_index_flags;

typedef enum {
	DUC_SIZE_TYPE_APPARENT,
	DUC_SIZE_TYPE_ACTUAL,
} duc_size_type;

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

typedef enum {
	DUC_FILE_TYPE_BLK,
	DUC_FILE_TYPE_CHR,
	DUC_FILE_TYPE_DIR,
	DUC_FILE_TYPE_FIFO,
	DUC_FILE_TYPE_LNK,
	DUC_FILE_TYPE_REG,
	DUC_FILE_TYPE_SOCK,
	DUC_FILE_TYPE_UNKNOWN,
} duc_file_type;

struct duc_devino {
	dev_t dev;
	ino_t ino;
};

struct duc_size {
	off_t actual;
	off_t apparent;
};

struct duc_index_report {
	char path[PATH_MAX];        /* Indexed path */
	struct duc_devino devino;   /* Index top device id and inode number */
	struct timeval time_start;  /* Index start time */
	struct timeval time_stop;   /* Index finished time */
	size_t file_count;          /* Total number of files indexed */
	size_t dir_count;           /* Total number of directories indexed */
	struct duc_size size;       /* Total size */
};

struct duc_dirent {
	char name[MAXNAMLEN];       /* File name */
	duc_file_type type;         /* File type */
	struct duc_size size;       /* File size */
	struct duc_devino devino;   /* Device id and inode number */
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

typedef void (*duc_index_progress_cb)(struct duc_index_report *report, void *ptr);

duc_index_req *duc_index_req_new(duc *duc);
int duc_index_req_add_exclude(duc_index_req *req, const char *pattern);
int duc_index_req_set_maxdepth(duc_index_req *req, int maxdepth);
int duc_index_req_set_progress_cb(duc_index_req *req, duc_index_progress_cb fn, void *ptr);
struct duc_index_report *duc_index(duc_index_req *req, const char *path, duc_index_flags flags);
int duc_index_req_free(duc_index_req *req);
int duc_index_report_free(struct duc_index_report *rep);


/*
 * Querying the duc database
 */

struct duc_index_report *duc_get_report(duc *duc, size_t id);

duc_dir *duc_dir_open(duc *duc, const char *path);
duc_dir *duc_dir_openat(duc_dir *dir, const char *name);
duc_dir *duc_dir_openent(duc_dir *dir, struct duc_dirent *e);
struct duc_dirent *duc_dir_read(duc_dir *dir, duc_size_type st);
char *duc_dir_get_path(duc_dir *dir);
void duc_dir_get_size(duc_dir *dir, struct duc_size *size);
size_t duc_dir_get_count(duc_dir *dir);
struct duc_dirent *duc_dir_find_child(duc_dir *dir, const char *name);
int duc_dir_seek(duc_dir *dir, off_t offset);
int duc_dir_rewind(duc_dir *dir);
int duc_dir_close(duc_dir *dir);

/* 
 * Helper functions
 */

int duc_human_number(double v, int exact, char *buf, size_t maxlen);
int duc_human_size(struct duc_size *size, duc_size_type st, int exact, char *buf, size_t maxlen);
int duc_human_duration(struct timeval start, struct timeval end, char *buf, size_t maxlen);
void duc_log(struct duc *duc, duc_log_level lvl, const char *fmt, ...);
char duc_file_type_char(duc_file_type t);
char *duc_file_type_name(duc_file_type t);

#endif
