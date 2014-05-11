
#ifndef db_raw_h
#define db_raw_h

struct db_raw;

struct db_raw *db_raw_open(const char *path_db, int flags);
void db_raw_close(struct db_raw *db_raw);
int db_raw_put(struct db_raw *db_raw, const void *key, size_t key_len, const void *val, size_t val_len);
void *db_raw_get(struct db_raw *db_raw, const void *key, size_t key_len, size_t *val_len);

#endif

