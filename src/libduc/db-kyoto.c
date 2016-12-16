
#include "config.h"

#ifdef ENABLE_KYOTOCABINET

#include <stdlib.h>
#include <string.h>

#include <kclangc.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h> 

#include "duc.h"
#include "private.h"
#include "db.h"

struct db {
	KCDB* kdb;
};


duc_errno tcdb_to_errno(KCDB *kdb)
{
	return DUC_E_UNKNOWN;
}


struct db *db_open(const char *path_db, int flags, duc_errno *e)
{
	struct db *db;
	int compress = 0;

	uint32_t mode = KCOREADER;
	if(flags & DUC_OPEN_RW) mode |= KCOWRITER | KCOCREATE;
	if(flags & DUC_OPEN_COMPRESS) compress = 1;

	db = duc_malloc(sizeof *db);

	db->kdb = kcdbnew();

	db->kdb = kcdbnew();
	if(!db->kdb) {
		*e = DUC_E_DB_BACKEND;
		goto err1;
	}

	char fname[DUC_PATH_MAX];
	snprintf(fname, sizeof(fname), "%s.kct#opts=c", path_db);

	int r = kcdbopen(db->kdb, fname, mode);
	if(r == 0) {
		perror(kcecodename(kcdbecode(db->kdb)));
		*e = tcdb_to_errno(db->kdb);
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
	kcdbclose(db->kdb);
err2:
	kcdbdel(db->kdb);
err1:
	free(db);
	return NULL;
}


void db_close(struct db *db)
{
	kcdbclose(db->kdb);
	kcdbdel(db->kdb);
	free(db);
}


duc_errno db_put(struct db *db, const void *key, size_t key_len, const void *val, size_t val_len)
{
	int r = kcdbset(db->kdb, key, key_len, val, val_len);
	return (r==1) ? DUC_OK : DUC_E_UNKNOWN;
}


void *db_get(struct db *db, const void *key, size_t key_len, size_t *val_len)
{
	size_t vall;
	void *val = kcdbget(db->kdb, key, key_len, &vall);
	*val_len = vall;
	return val;
}

#endif

/*
 * End
 */
