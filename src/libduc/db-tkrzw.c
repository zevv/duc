
#include "config.h"

#ifdef ENABLE_TKRZW

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <tkrzw_langc.h>

#include "duc.h"
#include "private.h"
#include "db.h"

struct db {
	TkrzwDBM* hdb;
};

duc_errno tkrzwdb_to_errno(TkrzwDBM *hdb)
{

    TkrzwStatus status = tkrzw_get_last_status();
    printf("tkrzw_get_last_status() = %s\n",status.message);
	switch(status.code) {
	    /*
		case TCESUCCESS: return DUC_OK;
		case TCENOFILE:  return DUC_E_DB_NOT_FOUND;
		case TCENOPERM:  return DUC_E_PERMISSION_DENIED;
		case TCEOPEN:    return DUC_E_PERMISSION_DENIED;
		case TCEMETA:    return DUC_E_DB_CORRUPT;
		case TCERHEAD:   return DUC_E_DB_CORRUPT;
		case TCEREAD:    return DUC_E_DB_CORRUPT;
		case TCESEEK:    return DUC_E_DB_CORRUPT;
	    */
		default:         return DUC_E_UNKNOWN;
	}
}

struct db *db_open(const char *path_db, int flags, duc_errno *e)
{
	struct db *db;
	int compress = 0;
	int writeable = 0;
	char options[] = "dbm=HashDBM,file=StdFile";

	if (flags & DUC_OPEN_FORCE) { 
	    char trunc[] = ",truncate=true";
	    strcat(options,trunc);
	}

	if (flags & DUC_OPEN_RW) writeable = 1;

	if (flags & DUC_OPEN_COMPRESS) {
	    char comp[] = ",record_comp_mode=RECORD_COMP_LZ4";
	    strcat(options,comp);
	}

	db = duc_malloc(sizeof *db);

	db->hdb = tkrzw_dbm_open(path_db, writeable, options);
  
	size_t vall;
	char *version = db_get(db, "duc_db_version", 14, &vall);
	if (version) {
		if(strcmp(version, DUC_DB_VERSION) != 0) {
			*e = DUC_E_DB_VERSION_MISMATCH;
			goto err2;
		}
		free(version);
	} else {
		db_put(db, "duc_db_version", 14, DUC_DB_VERSION, strlen(DUC_DB_VERSION));
	}

	return db;

err2:
	tkrzw_dbm_close(db->hdb);
err1:
	free(db);
	return NULL;
}


void db_close(struct db *db)
{
	tkrzw_dbm_close(db->hdb);
	free(db);
}


duc_errno db_put(struct db *db, const void *key, size_t key_len, const void *val, size_t val_len)
{
	int r = tkrzw_dbm_set(db->hdb, key, key_len, val, val_len, 1);
	return (r==1) ? DUC_OK : DUC_E_UNKNOWN;
}


void *db_get(struct db *db, const void *key, size_t key_len, size_t *val_len)
{
	int vall;
	void *val = tkrzw_dbm_get(db->hdb, key, key_len, &vall);
	*val_len = vall;
	return val;
}

#endif

/*
 * End
 */
