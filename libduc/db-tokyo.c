
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <tcutil.h>
#include <tchdb.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h> 

#include "duc.h"
#include "db.h"

struct db {
	TCHDB* hdb;
};


struct db *db_open(const char *path_db, int flags)
{
	struct db *db;

	db = malloc(sizeof *db);
	assert(db);

	db->hdb = tchdbnew();

	uint32_t mode = 0;
	if(flags & DUC_OPEN_RO) mode |= HDBOREADER;
	if(flags & DUC_OPEN_RW) mode |= HDBOWRITER | HDBOCREAT;

	tchdbopen(db->hdb, path_db, mode);

	return db;
}


void db_close(struct db *db)
{
	tchdbclose(db->hdb);
	free(db);
}


int db_put(struct db *db, const void *key, size_t key_len, const void *val, size_t val_len)
{
	int r = tchdbput(db->hdb, key, key_len, val, val_len);
	return(r == 1);
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
