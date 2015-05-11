
#include "config.h"

#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <time.h>

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
		duc_log(duc, DUC_LOG_FTL, "Error opening: %s - %s", path_db, duc_strerror(duc));
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


static int humanize(double v, int exact, double scale, char *buf, size_t maxlen)
{
	char prefix[] = { '\0', 'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y' };
	char *p = prefix;

	if(exact || v < scale) {
		return snprintf(buf, maxlen, "%.0f", v);
	} else {
		while(v >= scale) {
			v /= scale;
			p ++;
		}
		return snprintf(buf, maxlen, "%.1f%c", v, *p);
	}
}


int duc_human_number(double v, int exact, char *buf, size_t maxlen)
{
	return humanize(v, exact, 1000, buf, maxlen);
}


int duc_human_size(struct duc_size *size, duc_size_type st, int exact, char *buf, size_t maxlen)
{
	double v = (st == DUC_SIZE_TYPE_APPARENT) ? size->apparent : size->actual;
	return humanize(v, exact, 1024, buf, maxlen);
}


int duc_human_duration(struct timeval start, struct timeval stop, char *buf, size_t maxlen) 
{
	double start_secs, stop_secs, secs;
	unsigned int days, hours, mins; 

	/* fixme: use timersub here */

	start_secs =start.tv_sec + (start.tv_usec / 1000000.0);  
	stop_secs = stop.tv_sec + (stop.tv_usec / 1000000.0);  
	secs = stop_secs - start_secs;

	days = secs / 86400;
	secs = secs - (days * 86400);
	hours = secs / 3600;
	secs = secs - (hours * 3600);
	mins = secs / 60;
	secs = secs - (mins * 60);

	int l;

	if (days) {
		l = snprintf(buf, maxlen, "%d days, %02d hours, %02d minutes, and %.2f seconds.", days, hours, mins, secs); 
	} else if (hours) {
		l = snprintf(buf, maxlen, "%02d hours, %02d minutes, and %.2f seconds.", hours, mins, secs);
	} else if (mins) {
		l = snprintf(buf, maxlen, "%02d minutes, and %.2f seconds.", mins, secs);
	} else {
		l = snprintf(buf, maxlen, "%f secs.", secs);
	}

	return l;
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

