
#include "config.h"

#ifdef ENABLE_TKRZW

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// For sizing the DBs on large large filesystems.
#include <sys/statvfs.h>

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
	switch(status.code) {
	case TKRZW_STATUS_SUCCESS:                 return DUC_OK;
	case TKRZW_STATUS_UNKNOWN_ERROR:           return DUC_E_UNKNOWN;
	case TKRZW_STATUS_SYSTEM_ERROR:            return DUC_E_DB_NOT_FOUND;
	case TKRZW_STATUS_NOT_IMPLEMENTED_ERROR:   return DUC_E_NOT_IMPLEMENTED;
	case TKRZW_STATUS_PRECONDITION_ERROR:      return DUC_E_NOT_IMPLEMENTED;
	case TKRZW_STATUS_INVALID_ARGUMENT_ERROR:  return DUC_E_NOT_IMPLEMENTED;
	case TKRZW_STATUS_CANCELED_ERROR:          return DUC_E_UNKNOWN;
	case TKRZW_STATUS_NOT_FOUND_ERROR:         return DUC_E_DB_NOT_FOUND;
	case TKRZW_STATUS_PERMISSION_ERROR:        return DUC_E_PERMISSION_DENIED;
	case TKRZW_STATUS_INFEASIBLE_ERROR:        return DUC_E_DB_BACKEND;
	case TKRZW_STATUS_DUPLICATION_ERROR:       return DUC_E_DB_CORRUPT;
	case TKRZW_STATUS_BROKEN_DATA_ERROR:       return DUC_E_DB_CORRUPT;
	case TKRZW_STATUS_NETWORK_ERROR:           return DUC_E_UNKNOWN;
	case TKRZW_STATUS_APPLICATION_ERROR:       return DUC_E_UNKNOWN;
	default:                                   return DUC_E_UNKNOWN;
	}
}

struct db *db_open(const char *path_db, int flags, duc_errno *e)
{
	struct db *db;
	int compress = 0;
	int writeable = 0;
	char options[256] = "dbm=HashDBM,file=StdFile,offset_width=5";

	if (flags & DUC_OPEN_FORCE) { 
	    char trunc[] = ",truncate=true";
	    strcat(options,trunc);
	}

	// Ideally we would know the filesystem here so we can scale things properly, but this is a major re-work of API, so for now just define some new DUC_FS_*" factors...
	if (flags & DUC_FS_BIG) {
	    char big[] = ",num_buckets=100000000";
	    strcat(options,big);
	}

	if (flags & DUC_FS_BIGGER) {
	    char bigger[] = ",num_buckets=1000000000";
	    strcat(options,bigger);
	}

	if (flags & DUC_FS_BIGGEST) {
	    char biggest[] = ",num_buckets=10000000000";
	    strcat(options,biggest);
	}

	if (flags & DUC_OPEN_RW) writeable = 1;
	if (flags & DUC_OPEN_COMPRESS) {
	    /* Do no compression for now, need to update configure tests first */
	    char comp[] = ",record_comp_mode=RECORD_COMP_LZ4";
	    strcat(options,comp);
	}

	db = duc_malloc(sizeof *db);

	db->hdb = tkrzw_dbm_open(path_db, writeable, options);
	if (!db->hdb) {
	    *e = tkrzwdb_to_errno(db->hdb);
	    goto err1;
	}

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
