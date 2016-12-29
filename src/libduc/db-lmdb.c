
#include "config.h"

#ifdef ENABLE_LMDB

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdint.h> 

#include <lmdb.h>

#include "duc.h"
#include "private.h"
#include "db.h"

struct db {
	MDB_env *env;
	MDB_dbi dbi;
	MDB_txn *txn;
};


struct db *db_open(const char *path_db, int flags, duc_errno *e)
{
	struct db *db;
	unsigned int env_flags = MDB_NOSUBDIR;
	unsigned int open_flags = 0;
	unsigned int txn_flags = 0;

	if(flags & DUC_OPEN_RW) {
		open_flags |= MDB_CREATE;
	} else {
		env_flags |= MDB_RDONLY;
		txn_flags |= MDB_RDONLY;
	}

	/* On 32 bit machines the maximum db size is limited to 1GB, for 64 bit
	 * machines we increase to 256 GB */

	size_t map_size = 1024u * 1024u * 1024u;
	if(sizeof(size_t) == 8) map_size *= 256u;

	db = duc_malloc(sizeof *db);

	int rc;

	rc = mdb_env_create(&db->env);
	if(rc != MDB_SUCCESS) goto out;

	rc = mdb_env_set_mapsize(db->env, map_size);
	if(rc != MDB_SUCCESS) goto out;

	rc = mdb_env_open(db->env, path_db, env_flags, 0664);
	if(rc != MDB_SUCCESS) goto out;

	rc = mdb_txn_begin(db->env, NULL, txn_flags, &db->txn);
	if(rc != MDB_SUCCESS) goto out;

	rc = mdb_open(db->txn, NULL, open_flags, &db->dbi);
	if(rc != MDB_SUCCESS) goto out;

	return db;
out:
	fprintf(stderr, "%s\n", mdb_strerror(rc));
	*e = DUC_E_DB_NOT_FOUND;
	return NULL;
}


void db_close(struct db *db)
{
	mdb_txn_commit(db->txn);
	mdb_dbi_close(db->env, db->dbi);
	mdb_env_close(db->env);
	free(db);
}


duc_errno db_put(struct db *db, const void *key, size_t key_len, const void *val, size_t val_len)
{
	MDB_val k, d;
	int rc;

	k.mv_size = key_len;
	k.mv_data = (void *)key;
	d.mv_size = val_len;
	d.mv_data = (void *)val;

	rc = mdb_put(db->txn, db->dbi, &k, &d, 0);
	if(rc != MDB_SUCCESS) {
		fprintf(stderr, "%s\n", mdb_strerror(rc));
		exit(1);
	}

	return DUC_OK;
}


void *db_get(struct db *db, const void *key, size_t key_len, size_t *val_len)
{
	MDB_val k, d;
	int rc;

	k.mv_size = key_len;
	k.mv_data = (void *)key;

	rc = mdb_get(db->txn, db->dbi, &k, &d);

	if(rc == MDB_SUCCESS) {
		*val_len = d.mv_size;
		void *data = malloc(d.mv_size);
		memcpy(data, d.mv_data, d.mv_size);
		return data;
	} else {
		*val_len = 0;
		return NULL;
	}
}

#endif

/*
 * End
 */
