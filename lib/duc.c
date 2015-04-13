
#include "config.h"

#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <time.h>
#include <glob.h>

#include "private.h"
#include "duc.h"
#include "db.h"


static void default_log_callback(duc_log_level level, const char *fmt, va_list va)
{
	vfprintf(stderr, fmt, va);
	fprintf(stderr, "\n");
}



duc *duc_new(void)
{
	duc *duc = duc_malloc(sizeof *duc);
	memset(duc, 0, sizeof *duc);
	duc->log_level = DUC_LOG_WRN;
	duc->log_callback = default_log_callback;

	return duc;
}


void duc_del(duc *duc)
{
	if(duc->db) duc_close(duc);
	free(duc);
}


void duc_set_log_level(duc *duc, duc_log_level level)
{
	duc->log_level = level;
}


void duc_set_log_callback(duc *duc, duc_log_callback cb)
{
	duc->log_callback = cb;
}


int duc_open(duc *duc, const char *path_db, duc_open_flags flags)
{
	char tmp[PATH_MAX];

	/* An empty path means check the ENV path instead */
	if(path_db == NULL) {
		path_db = getenv("DUC_DATABASE");
	}

	/* If the path is still empty, default to ~/.duc.db */
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

	duc_log(duc, DUC_LOG_INF, "%s database \"%s\"", 
			(flags & DUC_OPEN_RO) ? "Reading from" : "Writing to",
			path_db);

	duc->db = db_open(path_db, flags, &duc->err);
	if(duc->db == NULL) {
		duc_log(duc, DUC_LOG_WRN, "Error opening: %s - %s", path_db, duc_strerror(duc));
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


/* 
 * Scan db_dir_path and return the number of DBs found, or -1 for error, putting an array
 * db filenames into *dbs 
 */

size_t duc_find_dbs(const char *db_dir_path, glob_t *db_list) 
{
	size_t count;

	char tmp[256];
	sprintf(tmp,"%s/*.db", db_dir_path);
	glob(tmp, GLOB_TILDE_CHECK, NULL, db_list);

	count = db_list->gl_pathc;
	if (count < 1) {
		duc_log(NULL, DUC_LOG_WRN,"failed to find DBs in %s", tmp);
	}
	return count;
}


void duc_log(struct duc *duc, duc_log_level level, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);

	if(duc) {
		if(duc->log_callback && level <= duc->log_level) {
			duc->log_callback(level, fmt, va);
		}
	} else {
		default_log_callback(level, fmt, va);
	}

	va_end(va);
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
	case DUC_E_DB_CORRUPT:           return "Database corrupt and not usable"; break;
	case DUC_E_DB_VERSION_MISMATCH:  return "Database version mismatch"; break;
	case DUC_E_PATH_NOT_FOUND:       return "Requested path not found"; break;
	case DUC_E_PERMISSION_DENIED:    return "Permission denied"; break;
	case DUC_E_OUT_OF_MEMORY:        return "Out of memory"; break;
	case DUC_E_DB_TCBDBNEW:          return "Unable to create DB using tcbdbnew()"; break;
	case DUC_E_UNKNOWN:              break;
	}

	return "Unknown error, contact the author";
}


char *duc_human_size(off_t size)
{
	char prefix[] = { '\0', 'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y' };
	double v = size;
	char *p = prefix;

	char *s;
	if(size < 1024) {
		asprintf(&s, "%jd", size);
	} else {
		while(v >= 1024.0) {
			v /= 1024.0;
			p ++;
		}
		asprintf(&s, "%.1f%c", v, *p);
	}
	return s;
}


char *duc_human_duration(struct timeval start, struct timeval stop) 
{
	double start_secs, stop_secs, secs;
	unsigned int days, hours, mins; 
	// char human_time[80];

	start_secs=start.tv_sec + (start.tv_usec / 1000000.0);  
	stop_secs=stop.tv_sec + (stop.tv_usec / 1000000.0);  
	secs = stop_secs - start_secs;

	days = secs / 86400;
	secs = secs - (days * 86400);
	hours = secs / 3600;
	secs = secs - (hours * 3600);
	mins = secs / 60;
	secs = secs - (mins * 60);

	char *s;

	if (days) {
		asprintf(&s, "%d days, %02d hours, %02d minutes, and %.2f seconds.", days, hours, mins, secs); 
	}
	else if (hours) {
		asprintf(&s, "%02d hours, %02d minutes, and %.2f seconds.", hours, mins, secs);
	}
	else if (mins) {
		asprintf(&s, "%02d minutes, and %.2f seconds.", mins, secs);
	}
	else {
		asprintf(&s, "%.2f secs.", secs);
	}

	return s;
}

						 
void *duc_malloc(size_t s)
{
	void *p = malloc(s);
	if(p == NULL) {
		duc_log(NULL, DUC_LOG_FTL, "out of memory");
		exit(1);
	}
	return p;
}


void *duc_realloc(void *p, size_t s)
{
	void *p2 = realloc(p, s);
	if(p2 == NULL) {
		duc_log(NULL, DUC_LOG_FTL, "out of memory");
		exit(1);
	}
	return p2;
}


char *duc_strdup(const char *s)
{
	char *s2 = strdup(s);
	if(s2 == NULL) {
		duc_log(NULL, DUC_LOG_FTL, "out of memory");
		exit(1);
	}
	return s2;
}


void duc_free(void *p)
{
	free(p);
}


/*
 * End
 */

