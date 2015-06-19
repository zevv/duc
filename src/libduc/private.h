#ifndef duc_internal_h
#define duc_internal_h

#include "duc.h"

#define DUC_DB_VERSION "15"

struct duc {
	struct db *db;
	struct conf *conf;
	duc_errno err;
	duc_log_level log_level;
	duc_log_callback log_callback;
};

void *duc_malloc(size_t s);
void *duc_realloc(void *p, size_t s);
char *duc_strdup(const char *s);
void duc_free(void *p);

char *stripdir(const char *dir);

#endif

