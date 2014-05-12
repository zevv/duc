
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "wamb.h"
#include "wamb_internal.h"
#include "db.h"


struct wamb *wamb_open(const char *path_db, int flags)
{
	char tmp[PATH_MAX];
	
	struct wamb *wamb = malloc(sizeof *wamb);
	if(wamb == NULL) return NULL;

	if(path_db == NULL) {
		path_db = getenv("WAMB_DATABASE");
	}

	if(path_db == NULL) {
		char *home = getenv("HOME");
		if(home) {
			snprintf(tmp, sizeof tmp, "%s/.wamb.db", home);
			path_db = tmp;
		}
	}
	
	wamb->db = db_open(path_db, flags);
	if(wamb->db == NULL) {
		free(wamb);
		return NULL;
	}

	return wamb;
}



void wamb_close(struct wamb *wamb)
{
	if(wamb->db) {
		db_close(wamb->db);
	}
	free(wamb);
}



int wamb_root_write(struct wamb *wamb, const char *path, dev_t dev, ino_t ino)
{
	char key[PATH_MAX];
	char val[32];
	snprintf(key, sizeof key, "%s", path);
	snprintf(val, sizeof val, "%jd %jd", dev, ino);
	db_put(wamb->db, key, strlen(key), val, strlen(val));
	return 0;
}

/*
 * End
 */

