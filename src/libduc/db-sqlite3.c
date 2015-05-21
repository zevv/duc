
#include "config.h"

#ifdef ENABLE_SQLITE

#include <stdlib.h>
#include <string.h>

#include <sqlite3.h>
#include <stdbool.h>
#include <stdint.h> 

#include "duc.h"
#include "private.h"
#include "db.h"

struct db {
	sqlite3 *s;
};

struct db *db_open(const char *path_db, int flags, duc_errno *e)
{
	struct db *db;
	int sflags = 0;

	db = duc_malloc(sizeof *db);

	if(flags & DUC_OPEN_RW)
		sflags |= SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
	else
		sflags |= SQLITE_OPEN_READONLY;

	int r = sqlite3_open_v2(path_db, &db->s, sflags, NULL);
	if(r != SQLITE_OK) goto err1;

	/* sqlite3_open() does not always notice corrupt database files. We do a bugus query
	 * here to catch this error case */

	r = sqlite3_exec(db->s, "select bogus from bogus", 0, 0, 0);
	if(r != 1) goto err1;

	char *q = "create table blobs(key unique primary key, value)";
	sqlite3_exec(db->s, q, 0, 0, 0);
	
	q = "create index keys on blobs(key)";
	sqlite3_exec(db->s, q, 0, 0, 0);
	
	sqlite3_exec(db->s, "begin", 0, 0, 0);

	return db;
err1:
	free(db);
	*e = DUC_E_DB_CORRUPT;
	if(r == SQLITE_CANTOPEN) *e = DUC_E_DB_NOT_FOUND;
	return NULL;
}


void db_close(struct db *db)
{
	sqlite3_exec(db->s, "commit", 0, 0, 0);
	free(db);
}


duc_errno db_put(struct db *db, const void *key, size_t key_len, const void *val, size_t val_len)
{
	sqlite3_stmt *pStmt;
	char *q = "insert into blobs(key, value) values(?, ?)";

	sqlite3_prepare(db->s, q, -1, &pStmt, 0);
	sqlite3_bind_text(pStmt, 1, key, key_len, SQLITE_STATIC);
	sqlite3_bind_blob(pStmt, 2, val, val_len, SQLITE_STATIC);
	sqlite3_step(pStmt);
	sqlite3_finalize(pStmt);
	return DUC_OK;
}


void *db_get(struct db *db, const void *key, size_t key_len, size_t *val_len)
{
	sqlite3_stmt *pStmt;
	char *q = "select value from blobs where key = ?";
	char *val = NULL;

	sqlite3_prepare(db->s, q, -1, &pStmt, 0);
	sqlite3_bind_text(pStmt, 1, key, key_len, SQLITE_STATIC);

	int r = sqlite3_step(pStmt);
	if(r == SQLITE_ROW) {
		*val_len = sqlite3_column_bytes(pStmt, 0);
		val = duc_malloc(*val_len);
		memcpy(val, sqlite3_column_blob(pStmt, 0), *val_len);
	}
	sqlite3_finalize(pStmt);

	return val;
}

#endif

/*
 * End
 */
