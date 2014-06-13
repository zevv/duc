
#include "config.h"

#include <stdlib.h>
#include <string.h>

#include <tcutil.h>
#include <tcbdb.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h> 

#include "duc.h"
#include "private.h"
#include "db.h"

struct db {
	TCBDB* hdb;
};

struct db *db_open(const char *path_db, int flags, duc_errno *e)
{
	struct db *db;
	int compress = 0;

	uint32_t mode = HDBONOLCK | HDBOREADER;
	if(flags & DUC_OPEN_RW) mode |= HDBOWRITER | HDBOCREAT;
	if(flags & DUC_OPEN_COMPRESS) compress = 1;

	/* If we pass in the -f switch, force opening the DB no matter what */
	if(flags & DUC_OPEN_FORCE) { mode |= BDBOTRUNC; }

	db = duc_malloc(sizeof *db);

	db->hdb = tcbdbnew();
	if(!db->hdb) {
		*e = DUC_E_DB_TCBDBNEW;
		goto err1;
	}

	if(compress) {
		tcbdbtune(db->hdb, -1, -1, -1, -1, -1, BDBTDEFLATE);
	}

	int r = tcbdbopen(db->hdb, path_db, mode);
	if(r == 0) {
	    *e = DUC_E_DB_CORRUPT;
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
	tcbdbclose(db->hdb);
err2:
	tcbdbdel(db->hdb);
err1:
	free(db);
	return NULL;
}


void db_close(struct db *db)
{
	tcbdbclose(db->hdb);
	tcbdbdel(db->hdb);
	free(db);
}


duc_errno db_put(struct db *db, const void *key, size_t key_len, const void *val, size_t val_len)
{
	int r = tcbdbput(db->hdb, key, key_len, val, val_len);
	return (r==1) ? DUC_OK : DUC_E_UNKNOWN;
}


void *db_get(struct db *db, const void *key, size_t key_len, size_t *val_len)
{
	int vall;
	void *val = tcbdbget(db->hdb, key, key_len, &vall);
	*val_len = vall;
	return val;
}


/*
 * End
 */
