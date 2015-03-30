#ifndef duc_internal_h
#define duc_internal_h

#include "duc.h"

#define DUC_DB_VERSION "10"

#ifndef HAVE_LSTAT
#define lstat stat
#endif

#ifndef S_ISLNK
#define S_ISLNK(v) 0
#endif

#ifndef S_ISSOCK
#define S_ISSOCK(v) 0
#endif

struct duc {
	struct db *db;
	duc_errno err;
	duc_log_level log_level;
	duc_log_callback log_callback;
};


struct duc_dir {
	struct duc *duc;
	dev_t dev;
	ino_t ino;
	dev_t dev_parent;
	ino_t ino_parent;
	char *path;
	struct duc_dirent *ent_list;
	off_t size_total;
	size_t file_count;
	size_t dir_count;
	size_t ent_cur;
	size_t ent_count;
	size_t ent_pool;
};

void *duc_malloc(size_t s);
void *duc_realloc(void *p, size_t s);
char *duc_strdup(const char *s);

struct duc_dir *duc_dir_new(struct duc *duc, dev_t dev, ino_t ino);
int duc_dir_add_ent(struct duc_dir *dir, const char *name, off_t size, uint8_t type, dev_t dev, ino_t ino);

char *stripdir(const char *dir);

#endif

