
#ifndef db_h
#define db_h

#include "private.h"
#include "duc.h"

struct db;

struct db *db_open(const char *path_db, int flags, duc_errno *e);
void db_close(struct db *db);
duc_errno db_put(struct db *db, const void *key, size_t key_len, const void *val, size_t val_len);
void db_free(void *ptr);
void *db_get(struct db *db, const void *key, size_t key_len, size_t *val_len);


duc_errno db_write_dir(struct duc_dir *dir);
struct duc_dir *db_read_dir(struct duc *duc, struct duc_devino *di);

#endif

