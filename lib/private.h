#ifndef duc_internal_h
#define duc_internal_h

#include "duc.h"

#define DUC_DB_VERSION "8"

#ifndef HAVE_LSTAT
#define lstat stat
#endif

#ifndef HAVE_REALPATH
char *realpath(const char *path, char resolved_path[PATH_MAX]);
#endif

typedef enum {
	LG_FTL,
	LG_WRN,
	LG_INF,
	LG_DBG,
	LG_DMP
} duc_loglevel;


struct duc {
	struct db *db;
	duc_errno err;
	duc_loglevel loglevel;
};


struct duc_dir {
	char *path;
	struct duc *duc;
	struct duc_dirent *ent_list;
	off_t size_total;
	size_t ent_cur;
	size_t ent_count;
	size_t ent_pool;
};

void *duc_malloc(size_t s);
void *duc_realloc(void *p, size_t s);
char *duc_strdup(const char *s);

void duc_log(struct duc *duc, duc_loglevel lvl, const char *fmt, ...);

struct duc_dir *duc_dir_new(struct duc *duc, size_t ent_max);
int duc_dir_add_ent(struct duc_dir *dir, const char *name, off_t size, mode_t mode, dev_t dev, ino_t ino);

int duc_db_write_dir(struct duc_dir *dir, dev_t dev, ino_t ino);
struct duc_dir *duc_db_read_dir(struct duc *duc, dev_t dev, ino_t ino);

#endif

