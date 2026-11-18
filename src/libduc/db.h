
#ifndef db_h
#define db_h

#include "duc.h"

/* Keys of the records that are neither directories nor index reports */

#define DB_KEY_VERSION "duc_db_version"
#define DB_KEY_REPORTS "duc_index_reports"

struct db;

struct db *db_open(const char *path_db, int flags, duc_errno *e);
void db_close(struct db *db);
duc_errno db_put(struct db *db, const void *key, size_t key_len, const void *val, size_t val_len);
void *db_get(struct db *db, const void *key, size_t key_len, size_t *val_len);


duc_errno db_write_report(duc *duc, const struct duc_index_report *rep);
struct duc_index_report *db_read_report(duc *duc, const char *path);

#endif

