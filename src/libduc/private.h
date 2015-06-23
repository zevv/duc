#ifndef duc_internal_h
#define duc_internal_h

#include "duc.h"

#define DUC_DB_VERSION "16"

#ifdef WIN32
#define FNAME_DUC_DB "duc.db"
#else
#define FNAME_DUC_DB ".duc.db"
#endif

struct duc {
	struct db *db;
	struct conf *conf;
	duc_errno err;
	duc_log_level log_level;
	duc_log_callback log_callback;
};

void *duc_malloc(size_t s);
void *duc_malloc0(size_t s);
void *duc_realloc(void *p, size_t s);
char *duc_strdup(const char *s);
void duc_free(void *p);


void duc_size_accum(struct duc_size *s1, struct duc_size *s2);
char *stripdir(const char *dir);

#endif

