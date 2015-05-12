
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


struct duc_dir *duc_dir_new(struct duc *duc, struct duc_devino *devino)
{
	struct duc_dir *dir = duc_malloc(sizeof(struct duc_dir));
	memset(dir, 0, sizeof *dir);

	dir->duc = duc;
	dir->devino.dev = devino->dev;
	dir->devino.ino = devino->ino;
	dir->path = NULL;
	dir->ent_pool = 32768;
	dir->ent_list = duc_malloc(dir->ent_pool);
	dir->size_type = -1;

	return dir;
}


int duc_dir_add_ent(struct duc_dir *dir, const char *name, struct duc_size *size, uint8_t type, struct duc_devino *devino)
{
	if((dir->ent_count+1) * sizeof(struct duc_dirent) > dir->ent_pool) {
		dir->ent_pool *= 2;
		dir->ent_list = duc_realloc(dir->ent_list, dir->ent_pool);
	}

	struct duc_dirent *ent = &dir->ent_list[dir->ent_count];
	dir->ent_count ++;

	snprintf(ent->name, sizeof(ent->name), "%s", name);
	ent->type = type;
	ent->size = *size;
	ent->devino = *devino;

	return 0;
}


void duc_dir_get_size(duc_dir *dir, struct duc_size *size)
{
	*size = dir->size;
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
	duc_dir *dir2 = db_read_dir(dir->duc, &e->devino);
	if(dir2) {
		asprintf(&dir2->path, "%s/%s", dir->path, e->name);
	}
	return dir2;
}


duc_dir *duc_dir_openat(duc_dir *dir, const char *name)
{
	if(strcmp(name, "..") == 0) {
		
		/* Special case: go up one directory */

		if(dir->devino_parent.dev && dir->devino_parent.ino) {
			duc_dir *pdir = db_read_dir(dir->duc, &dir->devino_parent);
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

	char *path_canon = stripdir(path);
	if(!path_canon) {
		duc->err = DUC_E_PATH_NOT_FOUND;
		return NULL;
	}

	/* Find top path in database */

	int l = strlen(path_canon);
	struct duc_devino devino = { 0, 0 };
	while(l > 0) {
		struct duc_index_report *report;
		size_t report_len;
		report = db_get(duc->db, path_canon, l, &report_len);
		if(report && report_len == sizeof(*report)) {
			devino = report->devino;
			free(report);
			break;
		}
		l--;
		while(l > 1  && path_canon[l] != '/') l--;
	}

	if(l == 0) {
		duc_log(duc, DUC_LOG_FTL, "Path %s not found in database", path_canon);
		duc->err = DUC_E_PATH_NOT_FOUND;
		free(path_canon);
		return NULL;
	}

	struct duc_dir *dir;

	dir = db_read_dir(duc, &devino);

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

	free(path_canon);

	return dir;
}


static int fn_comp_apparent(const void *a, const void *b)
{
	const struct duc_dirent *ea = a;
	const struct duc_dirent *eb = b;
	const struct duc_size *sa = &ea->size;
	const struct duc_size *sb = &eb->size;
	if(sa->apparent != sb->apparent) return(sa->apparent < sb->apparent);
	if(sa->actual != sb->actual) return(sa->actual < sb->actual);
	return strcmp(ea->name, eb->name);
}


static int fn_comp_actual(const void *a, const void *b)
{
	const struct duc_dirent *ea = a;
	const struct duc_dirent *eb = b;
	const struct duc_size *sa = &ea->size;
	const struct duc_size *sb = &eb->size;
	if(sa->actual != sb->actual) return(sa->actual < sb->actual);
	if(sa->apparent != sb->apparent) return(sa->apparent < sb->apparent);
	return strcmp(ea->name, eb->name);
}


struct duc_dirent *duc_dir_read(duc_dir *dir, duc_size_type st)
{
	int (*fn_comp)(const void *, const void *);

	dir->duc->err = 0;
		
	if(dir->size_type != st) {
		fn_comp = (st == DUC_SIZE_TYPE_APPARENT) ? fn_comp_apparent : fn_comp_actual;
		qsort(dir->ent_list, dir->ent_count, sizeof(struct duc_dirent), fn_comp);
		dir->size_type = st;
	}

	if(dir->ent_cur < dir->ent_count) {
		struct duc_dirent *ent = &dir->ent_list[dir->ent_cur];
		dir->ent_cur ++;
		return ent;
	} else {
		return NULL;
	}
}


int duc_dir_seek(duc_dir *dir, off_t offset)
{
	if(offset >= 0 && offset < dir->ent_count) {
		dir->ent_cur = offset;
		return 0;
	} else {
		return -1;
	}
}


int duc_dir_rewind(duc_dir *dir)
{
	return duc_dir_seek(dir, 0);
}


int duc_dir_close(duc_dir *dir)
{
	if(dir->path) free(dir->path);
	free(dir->ent_list);
	free(dir);
	return 0;
}

/*
 * End
 */

