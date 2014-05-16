
#include "config.h"

#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

#include "duc.h"
#include "duc-private.h"
#include "db.h"

duc *duc_new(void)
{
	duc *duc = malloc(sizeof *duc);
	if(!duc) return NULL;
	memset(duc, 0, sizeof *duc);
	return duc;
}


void duc_del(duc *duc)
{
	if(duc->db) duc_close(duc);
	free(duc);
}


int duc_open(duc *duc, const char *path_db, int flags)
{
	char tmp[PATH_MAX];

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
	
	if(path_db == NULL) {
		duc->err = DUC_E_DB_NOT_FOUND;
		return -1;
	}

	if(flags & DUC_OPEN_LOG_WRN) duc->loglevel = LG_WRN;
	if(flags & DUC_OPEN_LOG_INF) duc->loglevel = LG_INF;
	if(flags & DUC_OPEN_LOG_DBG) duc->loglevel = LG_DBG;

	duc->db = db_open(path_db, flags, &duc->err);
	if(duc->db == NULL) {
	  duc_log(duc, LG_WRN, "Error opening: %s - %s\n", path_db, duc_strerror(duc));
		free(duc);
		return -1;
	}

	return 0;
}


int duc_close(struct duc *duc)
{
	if(duc->db) {
		db_close(duc->db);
		duc->db = NULL;
	}
	return 0;
}


void duc_log(struct duc *duc, duc_loglevel level, const char *fmt, ...)
{
	if(level <= duc->loglevel) {
		va_list va;
		va_start(va, fmt);
		vfprintf(stderr, fmt, va);
		va_end(va);
	}
}


duc_errno duc_error(duc *duc)
{
	return duc->err;
}


const char *duc_strerror(duc *duc)
{
	switch(duc->err) {

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


void duc_humanize(off_t size, char *buf, size_t buflen)
{
	char prefix[] = { '\0', 'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y' };
	double v = size;
	char *p = prefix;

	if(size < 1024) {
		snprintf(buf, buflen, "%jd", size);
	} else {
		while(v >= 1024.0) {
			v /= 1024.0;
			p ++;
		}
		snprintf(buf, buflen, "%.1f%c", v, *p);
	}

}

						 

/*
 * End
 */

