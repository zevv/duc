
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <libgen.h>

#include "duc.h"
#include "db.h"
#include "buffer.h"
#include "private.h"


struct duc_dir {
	struct duc *duc;
	struct duc_devino devino;
	struct duc_devino devino_parent;
	time_t mtime;
	char *path;
	struct duc_dirent *ent_list;
	struct duc_size size;
	size_t ent_cur;
	size_t ent_count;
	size_t ent_pool;
	duc_size_type size_type;
};


struct duc_dir *duc_dir_new(struct duc *duc, const struct duc_devino *devino)
{
	size_t vall;
	char key[32];
	size_t keyl = snprintf(key, sizeof(key), "%jx/%jx", (uintmax_t)devino->dev, (uintmax_t)devino->ino);
	char *val = db_get(duc->db, key, keyl, &vall);
	if(val == NULL) {
		duc->err = DUC_E_PATH_NOT_FOUND;
		return NULL;
	}

	struct duc_dir *dir = duc_malloc0(sizeof(struct duc_dir));

	dir->duc = duc;
	dir->devino.dev = devino->dev;
	dir->devino.ino = devino->ino;
	dir->path = NULL;
	dir->ent_pool = 32768;
	dir->ent_list = duc_malloc(dir->ent_pool);
	dir->size_type = -1;

	struct buffer *b = buffer_new(val, vall);

	/* Read dir header */

	buffer_get_dir(b, &dir->devino_parent, &dir->mtime);

	/* Read all dirents */

	while(b->ptr < b->len) {

		if((dir->ent_count+1) * sizeof(struct duc_dirent) > dir->ent_pool) {
			dir->ent_pool *= 2;
			dir->ent_list = duc_realloc(dir->ent_list, dir->ent_pool);
		}

		struct duc_dirent *ent = &dir->ent_list[dir->ent_count];
		buffer_get_dirent(b, ent);

		duc_size_accum(&dir->size, &ent->size);
		dir->ent_count ++;
	}

	buffer_free(b);

	return dir;
}


void duc_dir_get_size(duc_dir *dir, struct duc_size *size)
{
	*size = dir->size;
}


size_t duc_dir_get_count(duc_dir *dir)
{
	return dir->ent_count;
}


char *duc_dir_get_path(duc_dir *dir)
{
	return strdup(dir->path);
}


duc_dir *duc_dir_openent(duc_dir *dir, const struct duc_dirent *e)
{
	duc_dir *dir2 = duc_dir_new(dir->duc, &e->devino);
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
			duc_dir *pdir = duc_dir_new(dir->duc, &dir->devino_parent);
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

	char *path_canon = duc_canonicalize_path(path);
	if(!path_canon) {
		duc->err = DUC_E_PATH_NOT_FOUND;
		return NULL;
	}

	/* Find top path in database */

	char *path_try = duc_strdup(path_canon);
	int l = strlen(path_try);
	struct duc_devino devino = { 0, 0 };
	while(l > 0) {
		path_try[l] = '\0';
		struct duc_index_report *report;
		report = db_read_report(duc, path_try);
		if(report) {
			devino = report->devino;
			free(report);
			break;
		}
		l--;
	}
	free(path_try);

	if(l == 0) {
		duc_log(duc, DUC_LOG_FTL, "Path %s not found in database", path_canon);
		duc->err = DUC_E_PATH_NOT_FOUND;
		free(path_canon);
		return NULL;
	}

	struct duc_dir *dir;

	dir = duc_dir_new(duc, &devino);

	if(dir == NULL) {
		duc->err = DUC_E_PATH_NOT_FOUND;
		free(path_canon);
		return NULL;
	}
	
	char rest[DUC_PATH_MAX];
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
	if(sa->apparent < sb->apparent) return +1;
	if(sa->apparent > sb->apparent) return -1;
	if(sa->actual < sb->actual) return +1;
	if(sa->actual > sb->actual) return -1;
	return strcmp(ea->name, eb->name);
}


static int fn_comp_actual(const void *a, const void *b)
{
	const struct duc_dirent *ea = a;
	const struct duc_dirent *eb = b;
	const struct duc_size *sa = &ea->size;
	const struct duc_size *sb = &eb->size;
	if(sa->actual < sb->actual) return +1;
	if(sa->actual > sb->actual) return -1;
	if(sa->apparent < sb->apparent) return +1;
	if(sa->apparent > sb->apparent) return -1;
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


int duc_dir_seek(duc_dir *dir, size_t offset)
{
	if(offset < dir->ent_count) {
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
	size_t i;
	for(i=0; i<dir->ent_count; i++) {
		free(dir->ent_list[i].name);
	}
	free(dir->ent_list);
	free(dir);
	return 0;
}


struct duc_index_report *duc_get_report(duc *duc, size_t id)
{
	size_t indexl;

	char *index = db_get(duc->db, "duc_index_reports", 17, &indexl);
	if(index == NULL) return NULL;

	size_t report_count = indexl / DUC_PATH_MAX;
	if(id >= report_count) return NULL;

	char *path = index + id * DUC_PATH_MAX;

	struct duc_index_report *r = db_read_report(duc, path);

	free(index);

	return r;
} 


/*
 * End
 */

