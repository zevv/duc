
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

#include "duc.h"
#include "duc-private.h"
#include "db.h"


struct duc *duc_open(const char *path_db, int flags, duc_errno *e)
{
	char tmp[PATH_MAX];
	
	struct duc *duc = malloc(sizeof *duc);
	if(duc == NULL) return NULL;
	memset(duc, 0, sizeof *duc);

	if(path_db == NULL) {
		path_db = getenv("DUC_DATABASE");
	}

	if(path_db == NULL) {
		char *home = getenv("HOME");
		if(home) {
			snprintf(tmp, sizeof tmp, "%s/.duc.db", home);
			path_db = tmp;
		}
	}

	if(flags & DUC_OPEN_LOG_WRN) duc->loglevel = LG_WRN;
	if(flags & DUC_OPEN_LOG_INF) duc->loglevel = LG_INF;
	if(flags & DUC_OPEN_LOG_DBG) duc->loglevel = LG_DBG;

	duc->db = db_open(path_db, flags, e);
	if(duc->db == NULL) {
		duc_log(duc, LG_WRN, "Error opening database: %s\n", duc_strerror(*e));
		free(duc);
		return NULL;
	}

	return duc;
}


void duc_close(struct duc *duc)
{
	if(duc->db) {
		db_close(duc->db);
	}
	free(duc);
}


void duc_log(struct duc *duc, enum duc_loglevel level, const char *fmt, ...)
{
	if(level <= duc->loglevel) {
		va_list va;
		va_start(va, fmt);
		vfprintf(stderr, fmt, va);
		va_end(va);
	}
}


int duc_root_write(struct duc *duc, const char *path, dev_t dev, ino_t ino)
{
	char key[PATH_MAX];
	char val[32];
	snprintf(key, sizeof key, "%s", path);
	snprintf(val, sizeof val, "%jd %jd", dev, ino);
	db_put(duc->db, key, strlen(key), val, strlen(val));
	return 0;
}


enum duc_errno duc_error(duc *duc)
{
	return duc->err;
}


const char *duc_strerror(duc_errno e)
{
	switch(e) {

		case DUC_OK:                     return "No error: success"; break;
		case DUC_E_DB_NOT_FOUND:         return "Database not found"; break;
		case DUC_E_DB_VERSION_MISMATCH:  return "Database version mismatch"; break;
		case DUC_E_PATH_NOT_FOUND:       return "Requested path not found"; break;
		case DUC_E_PERMISSION_DENIED:    return "Permission denied"; break;
		case DUC_E_OUT_OF_MEMORY:        return "Out of memory"; break;
		case DUC_E_UNKNOWN:              break;
	}

	return "Unknown error, contact the author";
}
						 

/*
 * End
 */

