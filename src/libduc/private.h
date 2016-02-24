#ifndef duc_internal_h
#define duc_internal_h

#include "duc.h"

#define DUC_DB_VERSION "17"

#ifndef S_ISLNK
#define S_ISLNK(v) 0
#endif

#ifndef S_ISSOCK
#define S_ISSOCK(v) 0
#endif

#ifndef HAVE_LSTAT
#define lstat stat
#endif

#ifndef timeradd
# define timeradd(a, b, result)			      \
  do {							      \
   (result)->tv_sec = (a)->tv_sec + (b)->tv_sec;	      \
   (result)->tv_usec = (a)->tv_usec + (b)->tv_usec;	      \
   if ((result)->tv_usec >= 1000000)			      \
     {							      \
       ++(result)->tv_sec;				      \
       (result)->tv_usec -= 1000000;			      \
     }							      \
  } while (0)
#endif

#ifdef WIN32
#define FNAME_DUC_DB "duc.db"
#else
#define FNAME_DUC_DB ".duc.db"
#endif

struct duc {
	struct db *db;
	duc_errno err;
	duc_log_level log_level;
	duc_log_callback log_callback;
};

void *duc_malloc(size_t s);
void *duc_malloc0(size_t s);
void *duc_realloc(void *p, size_t s);
char *duc_strdup(const char *s);
void duc_free(void *p);


void duc_size_accum(struct duc_size *s1, const struct duc_size *s2);
char *duc_canonicalize_path(const char *dir);

#endif

