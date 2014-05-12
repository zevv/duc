
#ifndef db_h
#define db_h

struct db;

struct db *db_open(const char *path_db, int flags);
void db_close(struct db *db);
int db_put(struct db *db, const void *key, size_t key_len, const void *val, size_t val_len);
void *db_get(struct db *db, const void *key, size_t key_len, size_t *val_len);

#endif

