
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <tcutil.h>
#include <tchdb.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h> 

#include "duc.h"
#include "duc-private.h"
#include "db.h"

struct db {
	TCHDB* hdb;
};

struct db *db_open(const char *path_db, int flags, duc_errno *e)
{
	struct db *db;
	int compress = 0;

	uint32_t mode = HDBONOLCK | HDBOREADER;
	if(flags & DUC_OPEN_RW) mode |= HDBOWRITER | HDBOCREAT;
	if(flags & DUC_OPEN_COMPRESS) compress = 1;


	db = malloc(sizeof *db);
	assert(db);

	db->hdb = tchdbnew();
	if(!db->hdb) {
		*e = DUC_E_UNKNOWN;
		goto err1;
	}

	if(compress) {
		tchdbtune(db->hdb, -1, -1, -1, HDBTDEFLATE);
	}

	int r = tchdbopen(db->hdb, path_db, mode);
	if(r == 0) {
		*e = DUC_E_DB_NOT_FOUND;
		goto err2;
	}

	size_t vall;
	char *version = db_get(db, "duc_db_version", 14, &vall);
	if(version) {
		if(strcmp(version, DUC_DB_VERSION) != 0) {
			*e = DUC_E_DB_VERSION_MISMATCH;
			goto err3;
		}
		free(version);
	} else {
		db_put(db, "duc_db_version", 14, DUC_DB_VERSION, strlen(DUC_DB_VERSION));
	}

	return db;

err3:
	tchdbclose(db->hdb);
err2:
	tchdbdel(db->hdb);
err1:
	free(db);
	return NULL;
}


void db_close(struct db *db)
{
	tchdbclose(db->hdb);
	tchdbdel(db->hdb);
	free(db);
}


duc_errno db_put(struct db *db, const void *key, size_t key_len, const void *val, size_t val_len)
{
	int r = tchdbput(db->hdb, key, key_len, val, val_len);
	return (r==1) ? DUC_OK : DUC_E_UNKNOWN;
}


duc_errno db_putcat(struct db *db, const void *key, size_t key_len, const void *val, size_t val_len)
{
	int r = tchdbputcat(db->hdb, key, key_len, val, val_len);
	return (r==1) ? DUC_OK : DUC_E_UNKNOWN;
}


void *db_get(struct db *db, const void *key, size_t key_len, size_t *val_len)
{
	int vall;
	void *val = tchdbget(db->hdb, key, key_len, &vall);
	*val_len = vall;
	return val;
}


/*
 * End
 */
