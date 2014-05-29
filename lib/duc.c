
#include "config.h"

#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <time.h>
#include <glob.h>

#include "duc.h"
#include "private.h"
#include "db.h"


duc *duc_new(void)
{
    duc *duc = duc_malloc(sizeof *duc);
    memset(duc, 0, sizeof *duc);
    return duc;
}


void duc_del(duc *duc)
{
	if(duc->db) duc_close(duc);
	free(duc);
}

/* Helper to pick a database */
char *duc_pick_db_path(const char *path_db) 
{
  char *tmp = NULL;

  if (path_db) {
	tmp = strdup(path_db);
  }

  if(tmp == NULL) {
	tmp = getenv("DUC_DATABASE");
  }
  
  if(tmp == NULL) {
	char *home = getenv("HOME");
	if(home) {
	  /* PATH_MAX is overkill, but memory is cheap... */
	  tmp = malloc(PATH_MAX);
	  tmp = memset(tmp, 0, PATH_MAX);
	  snprintf(tmp, PATH_MAX, "%s/.duc.db", home);
	}
  }
  return(tmp);
}

int duc_open(duc *duc, const char *path_db, duc_open_flags flags)
{
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

/* Scan db_dir_path and return the number of DBs found, or -1 for error, putting an array
   db filenames into *dbs */
size_t duc_find_dbs(const char *db_dir_path, glob_t *db_list) 
{
    size_t count;

    char tmp[256];
    sprintf(tmp,"%s/*.db", db_dir_path);
    glob(tmp, GLOB_TILDE_CHECK, NULL, db_list);

    count = db_list->gl_pathc;
    if (count < 1) {
        fprintf(stderr,"failed to find DBs in %s\n", tmp);
    }
    return count;
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

void duc_fmttime(char *human, struct timeval start, struct timeval stop) {

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
  
  if (days) {
	sprintf(human,"%d days, %02d hours, %02d minutes, and %.2f seconds.", days, hours, mins, secs); 
  }
  else if (hours) {
	sprintf(human,"%02d hours, %02d minutes, and %.2f seconds.", hours, mins, secs);
  }
  else if (mins) {
	sprintf(human,"%02d minutes, and %.2f seconds.", mins, secs);
  }
  else {
	sprintf(human,"%.2f secs.", secs);
  }
}

						 
void *duc_malloc(size_t s)
{
	void *p = malloc(s);
	if(p == NULL) {
		fprintf(stderr, "out of memory");
		exit(1);
	}
	return p;
}


void *duc_realloc(void *p, size_t s)
{
	void *p2 = realloc(p, s);
	if(p2 == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(1);
	}
	return p2;
}


char *duc_strdup(const char *s)
{
	char *s2 = strdup(s);
	if(s2 == NULL) {
		fprintf(stderr, "out of memory");
		exit(1);
	}
	return s2;
}


/*
 * End
 */

