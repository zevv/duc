
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "duc.h"
#include "duc-private.h"
#include "db.h"


struct duc *duc_open(const char *path_db, int flags)
{
	char tmp[PATH_MAX];
	
	struct duc *duc = malloc(sizeof *duc);
	if(duc == NULL) return NULL;

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
	
	duc->db = db_open(path_db, flags);
	if(duc->db == NULL) {
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



int duc_root_write(struct duc *duc, const char *path, dev_t dev, ino_t ino)
{
	char key[PATH_MAX];
	char val[32];
	snprintf(key, sizeof key, "%s", path);
	snprintf(val, sizeof val, "%jd %jd", dev, ino);
	db_put(duc->db, key, strlen(key), val, strlen(val));
	return 0;
}

/*
 * End
 */

