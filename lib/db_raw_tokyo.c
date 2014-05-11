
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <tcutil.h>
#include <tchdb.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h> 

#include "wamb.h"
#include "db.h"

struct db_raw {
	TCHDB* hdb;
};


struct db_raw *db_raw_open(const char *path_db, int flags)
{
	struct db_raw *db_raw;

	db_raw = malloc(sizeof *db_raw);
	assert(db_raw);

	db_raw->hdb = tchdbnew();

	uint32_t mode = 0;
	if(flags & WAMB_OPEN_RO) mode |= HDBOREADER;
	if(flags & WAMB_OPEN_RW) mode |= HDBOWRITER | HDBOCREAT;

	tchdbopen(db_raw->hdb, path_db, mode);

	return db_raw;
}


void db_raw_close(struct db_raw *db_raw)
{
	tchdbclose(db_raw->hdb);
	free(db_raw);
}


int db_raw_put(struct db_raw *db_raw, const void *key, size_t key_len, const void *val, size_t val_len)
{
	int r = tchdbput(db_raw->hdb, key, key_len, val, val_len);
	return(r == 1);
}


void *db_raw_get(struct db_raw *db_raw, const void *key, size_t key_len, size_t *val_len)
{
	int vall;
	void *val = tchdbget(db_raw->hdb, key, key_len, &vall);
	*val_len = vall;
	return val;
}


/*
 * End
 */
