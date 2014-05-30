
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdint.h>
#include <libgen.h>

#include "duc.h"
#include "db.h"
#include "private.h"


struct duc_dir *duc_dir_new(struct duc *duc, dev_t dev, ino_t ino)
{
	struct duc_dir *dir = duc_malloc(sizeof(struct duc_dir));
	memset(dir, 0, sizeof *dir);

	dir->duc = duc;
	dir->dev = dev;
	dir->ino = ino;
	dir->path = NULL;
	dir->ent_cur = 0;
	dir->ent_count = 0;
	dir->size_total = 0;
	dir->ent_pool = 32768;
	dir->ent_list = duc_malloc(dir->ent_pool);

	return dir;
}


int duc_dir_add_ent(struct duc_dir *dir, const char *name, off_t size, mode_t mode, dev_t dev, ino_t ino)
{
	if((dir->ent_count+1) * sizeof(struct duc_dirent) > dir->ent_pool) {
		dir->ent_pool *= 2;
		dir->ent_list = duc_realloc(dir->ent_list, dir->ent_pool);
	}

	struct duc_dirent *ent = &dir->ent_list[dir->ent_count];
	dir->ent_count ++;

	ent->name = duc_strdup(name);
	ent->size = size;
	ent->mode = mode;
	ent->dev = dev;
	ent->ino = ino;

	return 0;
}


off_t duc_dir_get_size(duc_dir *dir)
{
	return dir->size_total;
}


size_t duc_dir_get_count(duc_dir *dir)
{
	return dir->file_count + dir->dir_count;
}


char *duc_dir_get_path(duc_dir *dir)
{
	return strdup(dir->path);
}



duc_dir *duc_dir_openent(duc_dir *dir, struct duc_dirent *e)
{
	duc_dir *dir2 = db_read_dir(dir->duc, e->dev, e->ino);
	if(dir2) {
		asprintf(&dir2->path, "%s/%s", dir->path, e->name);
	}
	return dir2;
}


duc_dir *duc_dir_openat(duc_dir *dir, const char *name)
{
	if(strcmp(name, "..") == 0) {
		
		/* Special case: go up one directory */

		if(dir->dev_parent && dir->ino_parent) {
			duc_dir *pdir = db_read_dir(dir->duc, dir->dev_parent, dir->ino_parent);
			if(pdir == NULL) return NULL;
			pdir->path = duc_strdup(dir->path);
			dirname(pdir->path);
			return pdir;
		}

	} else {

		/* Find given name in dir */

		size_t i;
		struct duc_dirent *e = dir->ent_list;
		for(i=0; i<dir->ent_count; i++) {
			if(strcmp(e->name, name) == 0) {
				return duc_dir_openent(dir, e);
			}
			e++;
		}
	}

	return NULL;
}


int duc_dir_limit(duc_dir *dir, size_t count)
{
	if(dir->ent_count <= count) return 0;

	off_t rest_size = 0;
	off_t rest_count = dir->ent_count - count;

	size_t i;
	struct duc_dirent *ent = dir->ent_list;
	for(i=0; i<dir->ent_count; i++) {
		if(i>=count-1) rest_size += ent->size;
		ent++;
	}

	dir->ent_list = realloc(dir->ent_list, sizeof(struct duc_dirent) * count);
	dir->ent_count = count;
	dir->ent_cur = 0;
	ent = &dir->ent_list[count-1];

	asprintf(&ent->name, "(%ld files)", rest_count);
	ent->mode = DUC_MODE_REST;
	ent->size = rest_size;
	ent->dev = 0;
	ent->ino = 0;

	return 0;
}


struct duc_dirent *duc_dir_find_child(duc_dir *dir, const char *name)
{
	size_t i;
	struct duc_dirent *ent = dir->ent_list;

	for(i=0; i<dir->ent_count; i++) {
		if(strcmp(name, ent->name) == 0) {
			return ent;
		}
		ent++;
	}
	
	dir->duc->err = DUC_E_PATH_NOT_FOUND;
	return NULL;
}



duc_dir *duc_dir_open(struct duc *duc, const char *path)
{
	/* Canonicalized path */

	char *path_canon = realpath(path, NULL);
	if(path_canon == NULL) {
		duc->err = DUC_E_PATH_NOT_FOUND;
		return NULL;
	}

	/* Find top path in database */

	int l = strlen(path_canon);
	dev_t dev = 0;
	ino_t ino = 0;
	while(l > 0) {
		struct duc_index_report *report;
		size_t report_len;
		report = db_get(duc->db, path_canon, l, &report_len);
		if(report && report_len == sizeof(*report)) {
			dev = report->dev;
			ino = report->ino;
			free(report);
			break;
		}
		l--;
		while(l > 1  && path_canon[l] != '/') l--;
	}

	if(l == 0) {
		duc_log(duc, DUC_LOG_WRN, "Path %s not found in database\n", path_canon);
		duc->err = DUC_E_PATH_NOT_FOUND;
		free(path_canon);
		return NULL;
	}

	struct duc_dir *dir;

	dir = db_read_dir(duc, dev, ino);

	if(dir == NULL) {
		duc->err = DUC_E_PATH_NOT_FOUND;
		free(path_canon);
		return NULL;
	}
	
	char rest[PATH_MAX];
	strncpy(rest, path_canon+l, sizeof rest);

	char *name = strtok(rest, "/");

	while(dir && name) {

		struct duc_dirent *ent = duc_dir_find_child(dir, name);

		struct duc_dir *dir_next = NULL;

		if(ent) {
			dir_next = duc_dir_openent(dir, ent);
		}

		duc_dir_close(dir);
		dir = dir_next;
		name = strtok(NULL, "/");
	}

	if(dir) {
		dir->path = strdup(path_canon);
	}

	return dir;
}


struct duc_dirent *duc_dir_read(duc_dir *dir)
{
	dir->duc->err = 0;
	if(dir->ent_cur < dir->ent_count) {
		struct duc_dirent *ent = &dir->ent_list[dir->ent_cur];
		dir->ent_cur ++;
		return ent;
	} else {
		return NULL;
	}
}


int duc_dir_rewind(duc_dir *dir)
{
	dir->ent_cur = 0;
	return 0;
}


int duc_dir_close(duc_dir *dir)
{
	if(dir->path) free(dir->path);
	int i;
	for(i=0; i<dir->ent_count; i++) {
		free(dir->ent_list[i].name);
	}
	free(dir->ent_list);
	free(dir);
	return 0;
}

/*
 * End
 */

