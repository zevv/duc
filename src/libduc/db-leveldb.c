
#include "config.h"

#ifdef ENABLE_LEVELDB

#include <stdlib.h>
#include <string.h>

#include <leveldb/c.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h> 

#include "duc.h"
#include "private.h"
#include "db.h"

struct db {
	leveldb_t *db;
	leveldb_options_t *options;
	leveldb_readoptions_t *roptions;
	leveldb_writeoptions_t *woptions;
};

struct db *db_open(const char *path_db, int flags, duc_errno *e)
{
	struct db *db;
	char *err = NULL;

	db = duc_malloc(sizeof *db);

	db->options = leveldb_options_create();
	db->woptions = leveldb_writeoptions_create();
	db->roptions = leveldb_readoptions_create();

	leveldb_options_set_create_if_missing(db->options, 1);
	leveldb_options_set_compression(db->options, leveldb_snappy_compression);

	db->db = leveldb_open(db->options, path_db, &err);
	if (err != NULL) {
		*e = DUC_E_UNKNOWN;
		return(NULL);
	}

	return db;
}


void db_close(struct db *db)
{
	free(db);
}


duc_errno db_put(struct db *db, const void *key, size_t key_len, const void *val, size_t val_len)
{
	char *err;
	leveldb_put(db->db, db->woptions, key, key_len, val, val_len, &err);
	return err ? DUC_E_UNKNOWN : DUC_OK;
}


duc_errno db_putcat(struct db *db, const void *key, size_t key_len, const void *val, size_t val_len)
{
	void *t;
	size_t t_len;

	t = db_get(db, key, key_len, &t_len);
	if(t) {
		t = realloc(t, t_len + val_len);
		memcpy(t+t_len, val, t_len + val_len);
		db_put(db, key, key_len, t, t_len + val_len);
		free(t);
		return DUC_OK;
	} else {
		db_put(db, key, key_len, val, val_len);
		return DUC_OK;
	}
}


void *db_get(struct db *db, const void *key, size_t key_len, size_t *val_len)
{
	char *err;
	char *val = leveldb_get(db->db, db->roptions, key, key_len, val_len, &err);
	return val;
}

#endif

/*
 * End
 */
