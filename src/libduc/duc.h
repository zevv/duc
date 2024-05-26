
#ifndef duc_h
#define duc_h

#include <limits.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <dirent.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/time.h>

#define DUC_PATH_MAX 16384

#ifdef WIN32
typedef int64_t duc_dev_t;
typedef int64_t duc_ino_t;
#else
typedef dev_t duc_dev_t;
typedef ino_t duc_ino_t;
#endif

typedef struct duc duc;
typedef struct duc_dir duc_dir;
typedef struct duc_index_req duc_index_req;
typedef struct duc_histogram duc_histogram;

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
	DUC_INDEX_DRY_RUN          = 1<<3, /* Do not touch the database */
} duc_index_flags;

typedef enum {
	DUC_SIZE_TYPE_APPARENT,
	DUC_SIZE_TYPE_ACTUAL,
	DUC_SIZE_TYPE_COUNT,
} duc_size_type;

typedef enum {
	DUC_SORT_SIZE = 1,
	DUC_SORT_NAME = 2,
} duc_sort;

typedef enum {
	DUC_OK,                     /* No error, success */
	DUC_E_DB_NOT_FOUND,         /* Database not found */
	DUC_E_DB_CORRUPT,           /* Database corrupt and unusable */
	DUC_E_DB_VERSION_MISMATCH,  /* Database version mismatch */
	DUC_E_DB_TYPE_MISMATCH,     /* Database compiled in type mismatch */
	DUC_E_PATH_NOT_FOUND,       /* Requested path not found */
	DUC_E_PERMISSION_DENIED,    /* Permission denied */
	DUC_E_OUT_OF_MEMORY,        /* Out of memory */
	DUC_E_DB_BACKEND,           /* Unable to initialize database backend */
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
	duc_dev_t dev;
	duc_ino_t ino;
};

struct duc_size {
	off_t actual;
	off_t apparent;
	off_t count;
};

struct duc_index_report {
	char path[DUC_PATH_MAX];        /* Indexed path */
	struct duc_devino devino;   /* Index top device id and inode number */
	struct timeval time_start;  /* Index start time */
	struct timeval time_stop;   /* Index finished time */
	size_t file_count;          /* Total number of files indexed */
	size_t dir_count;           /* Total number of directories indexed */
	struct duc_size size;       /* Total size */
};

struct duc_dirent {
	char *name;                 /* File name */
	duc_file_type type;         /* File type */
	struct duc_size size;       /* File size */
	struct duc_devino devino;   /* Device id and inode number */
};

struct duc_histogram {
	size_t bins;
	struct duc_histogram_bin {
		size_t min;
		size_t max;
		size_t file_count;
		size_t dir_count;
	} *bin;
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
int duc_index_req_set_username(duc_index_req *req, const char *username);
int duc_index_req_set_uid(duc_index_req *req, int uid);
int duc_index_req_add_exclude(duc_index_req *req, const char *pattern);
int duc_index_req_add_fstype_include(duc_index_req *req, const char *types);
int duc_index_req_add_fstype_exclude(duc_index_req *req, const char *types);
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
duc_dir *duc_dir_openent(duc_dir *dir, const struct duc_dirent *e);
struct duc_dirent *duc_dir_read(duc_dir *dir, duc_size_type st, duc_sort sort);
char *duc_dir_get_path(duc_dir *dir);
void duc_dir_get_size(duc_dir *dir, struct duc_size *size);
size_t duc_dir_get_count(duc_dir *dir);
struct duc_dirent *duc_dir_find_child(duc_dir *dir, const char *name);
int duc_dir_seek(duc_dir *dir, size_t offset);
int duc_dir_rewind(duc_dir *dir);
int duc_dir_close(duc_dir *dir);

/*
 * Histogram
 */

duc_histogram *duc_histogram_new(duc_size_type st, size_t bin_min, size_t bin_max, double power);
void duc_histogram_accumumlate(duc_histogram *h, struct duc_dirent *e);
void duc_histogram_free(duc_histogram *h);


/* 
 * Helper functions
 */

off_t duc_get_size(struct duc_size *size, duc_size_type st);
int duc_human_number(double v, int exact, char *buf, size_t maxlen);
int duc_human_size(const struct duc_size *size, duc_size_type st, int exact, char *buf, size_t maxlen);
int duc_human_duration(struct timeval start, struct timeval end, char *buf, size_t maxlen);
void duc_log(struct duc *duc, duc_log_level lvl, const char *fmt, ...);
char duc_file_type_char(duc_file_type t);
char *duc_file_type_name(duc_file_type t);

char *duc_db_type_check(const char *path_db);
#endif  /* ifndef duc_h */
