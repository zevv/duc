
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdint.h>

#include "duc.h"
#include "db.h"
#include "buffer.h"
#include "duc-private.h"
#include "varint.h"


struct duc_dir *duc_dir_new(struct duc *duc, size_t ent_max)
{
	struct duc_dir *dir = malloc(sizeof(struct duc_dir));

	if(dir == NULL) {
		duc->err = DUC_E_OUT_OF_MEMORY;
		return NULL;
	}

	dir->path = NULL;
	dir->duc = duc;
	dir->ent_cur = 0;
	dir->ent_count = 0;
	dir->size_total = 0;
	dir->ent_max = ent_max;
	dir->ent_list = malloc(sizeof(struct duc_dirent) * ent_max);

	if(dir->ent_list == NULL) {
		duc->err = DUC_E_OUT_OF_MEMORY;
		free(dir);
		return NULL;
	}

	return dir;
}


int duc_dir_add_ent(struct duc_dir *dir, const char *name, off_t size, mode_t mode, dev_t dev, ino_t ino)
{
	if(dir->ent_count >= dir->ent_max) {
		dir->ent_max *= 2;
		dir->ent_list = realloc(dir->ent_list, sizeof(struct duc_dirent) * dir->ent_max);
		if(dir->ent_list == NULL) {
			dir->duc->err = DUC_E_OUT_OF_MEMORY;
			return -1;
		}
	}

	struct duc_dirent *ent = &dir->ent_list[dir->ent_count];
	dir->size_total += size;
	dir->ent_count ++;

	strncpy(ent->name, name, sizeof(ent->name));
	ent->size = size;
	ent->mode = mode;
	ent->dev = dev;
	ent->ino = ino;

	return 0;
}


static int fn_comp_ent(const void *a, const void *b)
{
	const struct duc_dirent *ea = a;
	const struct duc_dirent *eb = b;
	return(ea->size < eb->size);
}


static int mkkey(dev_t dev, ino_t ino, char *key, size_t keylen)
{
	return snprintf(key, keylen, "%jx/%jx", dev, ino);
}


off_t duc_dirsize(duc_dir *dir)
{
	return dir->size_total;
}


char *duc_dirpath(duc_dir *dir)
{
	return dir->path;
}


/*
 * Serialize duc_dir into a database record
 */

int duc_dir_write(struct duc_dir *dir, dev_t dev, ino_t ino)
{
	struct buffer *b = buffer_new(NULL, 0);
	if(b == NULL) {
		dir->duc->err = DUC_E_OUT_OF_MEMORY;
		return -1;
	}

	int i;
	struct duc_dirent *ent = dir->ent_list;

	for(i=0; i<dir->ent_count; i++) {
		buffer_put_string(b, ent->name);
		buffer_put_varint(b, ent->size);
		buffer_put_varint(b, ent->mode);
		buffer_put_varint(b, ent->dev);
		buffer_put_varint(b, ent->ino);
		ent++;
	}

	char key[32];
	size_t keyl = mkkey(dev, ino, key, sizeof key);
	int r = db_put(dir->duc->db, key, keyl, b->data, b->len);
	if(r != 0) {
		dir->duc->err = r;
		return -1;
	}

	buffer_free(b);

	return 0;
}


/*
 * Read database record and deserialize into duc_dir
 */

struct duc_dir *duc_dir_read(struct duc *duc, dev_t dev, ino_t ino)
{
	struct duc_dir *dir = duc_dir_new(duc, 8);
	if(dir == NULL) {
		duc->err = DUC_E_OUT_OF_MEMORY;
		return NULL;
	}

	char key[32];
	size_t keyl;
	size_t vall;

	keyl = mkkey(dev, ino, key, sizeof key);
	char *val = db_get(duc->db, key, keyl, &vall);
	if(val == NULL) {
		duc_log(duc, LG_WRN, "Key %s not found in database\n", key);
		duc->err = DUC_E_PATH_NOT_FOUND;
		return NULL;
	}

	struct buffer *b = buffer_new(val, vall);

	while(b->ptr < b->len) {

		uint64_t size;
		uint64_t dev;
		uint64_t ino;
		uint64_t mode;
		char *name;

		buffer_get_string(b, &name);
		buffer_get_varint(b, &size);
		buffer_get_varint(b, &mode);
		buffer_get_varint(b, &dev);
		buffer_get_varint(b, &ino);
	
		if(name) {
			duc_dir_add_ent(dir, name, size, mode, dev, ino);
			free(name);
		}
	}

	buffer_free(b);

	qsort(dir->ent_list, dir->ent_count, sizeof(struct duc_dirent), fn_comp_ent);

	return dir;
}


duc_dir *duc_opendirat(duc_dir *dir, struct duc_dirent *e)
{
	duc_dir *dir2 = duc_dir_read(dir->duc, e->dev, e->ino);
	if(dir2) {
		asprintf(&dir2->path, "%s/%s", dir->path, e->name);
	}
	return dir2;
}


int duc_limitdir(duc_dir *dir, size_t count)
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

	char s[16];
	duc_humanize(rest_count, s, sizeof s);
	snprintf(ent->name, sizeof(ent->name), "(%s files)", s);
	ent->mode = DUC_MODE_REST;
	ent->size = rest_size;
	ent->dev = 0;
	ent->ino = 0;

	return 0;
}


struct duc_dirent *duc_finddir(duc_dir *dir, const char *name)
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


duc_dir *duc_opendir(struct duc *duc, const char *path)
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
		duc_log(duc, LG_WRN, "Path %s not found in database\n", path_canon);
		duc->err = DUC_E_PATH_NOT_FOUND;
		free(path_canon);
		return NULL;
	}

	struct duc_dir *dir;

	dir = duc_dir_read(duc, dev, ino);

	if(dir == NULL) {
		duc->err = DUC_E_PATH_NOT_FOUND;
		free(path_canon);
		return NULL;
	}
	
	char rest[256];
	strncpy(rest, path_canon+l, sizeof rest);

	char *save;
	char *name = strtok_r(rest, "/", &save);

	while(dir && name) {

		struct duc_dirent *ent = duc_finddir(dir, name);

		struct duc_dir *dir_next = NULL;

		if(ent) {
			dir_next = duc_opendirat(dir, ent);
		}

		duc_closedir(dir);
		dir = dir_next;
		name = strtok_r(NULL, "/", &save);
	}

	if(dir) {
		dir->path = strdup(path_canon);
	}

	return dir;
}


struct duc_dirent *duc_readdir(duc_dir *dir)
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


int duc_rewinddir(duc_dir *dir)
{
	dir->ent_cur = 0;
	return 0;
}


int duc_closedir(duc_dir *dir)
{
	if(dir->path) free(dir->path);
	free(dir->ent_list);
	free(dir);
	return 0;
}

/*
 * End
 */

