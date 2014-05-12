
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdint.h>

#include "wamb.h"
#include "db.h"
#include "buffer.h"
#include "wamb_internal.h"
#include "varint.h"


struct wambdir {
	struct wamb *wamb;
	struct wambent *ent_list;
	mode_t mode;
	size_t ent_cur;
	size_t ent_count;
	size_t ent_max;
};


struct wambdir *wambdir_new(struct wamb *wamb, size_t ent_max)
{
	struct wambdir *dir = malloc(sizeof(struct wambdir));

	if(dir == NULL) {
		return NULL;
	}

	dir->wamb = wamb;
	dir->ent_cur = 0;
	dir->ent_count = 0;
	dir->ent_max = ent_max;
	dir->ent_list = malloc(sizeof(struct wambent) * ent_max);

	if(dir->ent_list == NULL) {
		free(dir);
		return NULL;
	}

	return dir;
}


void wambdir_add_ent(struct wambdir *dir, const char *name, size_t size, mode_t mode, dev_t dev, ino_t ino)
{
	if(dir->ent_count >= dir->ent_max) {
		dir->ent_max *= 2;
		dir->ent_list = realloc(dir->ent_list, sizeof(struct wambent) * dir->ent_max);
		assert(dir->ent_list);
	}

	struct wambent *ent = &dir->ent_list[dir->ent_count];
	dir->ent_count ++;

	strncpy(ent->name, name, sizeof(ent->name));
	ent->size = size;
	ent->mode = mode;
	ent->dev = dev;
	ent->ino = ino;
}


static int fn_comp_ent(const void *a, const void *b)
{
	const struct wambent *ea = a;
	const struct wambent *eb = b;
	return(ea->size < eb->size);
}


static int mkkey(dev_t dev, ino_t ino, char *key, size_t keylen)
{
	return snprintf(key, keylen, "%jd/%jd", dev, ino);
}


/*
 * Serialize wambdir into a database record
 */

int wambdir_write(struct wambdir *dir, dev_t dev, ino_t ino)
{
	struct buffer *b = buffer_new(NULL, 0);

	int i;
	struct wambent *ent = dir->ent_list;

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
	db_put(dir->wamb->db, key, keyl, b->data, b->len);

	buffer_free(b);

	return 0;
}


/*
 * Read database record and deserialize into wambdir
 */

struct wambdir *wambdir_read(struct wamb *wamb, dev_t dev, ino_t ino)
{
	struct wambdir *dir = wambdir_new(wamb, 8);
	if(dir == NULL) {
		return NULL;
	}

	char key[32];
	size_t keyl;
	size_t vall;

	keyl = mkkey(dev, ino, key, sizeof key);
	char *val = db_get(wamb->db, key, keyl, &vall);
	if(val == NULL) {
		fprintf(stderr, "Id %jd/%jd not found in database\n", dev, ino);
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
		
		wambdir_add_ent(dir, name, size, mode, dev, ino);
	}

	return dir;
}


wambdir *wamb_opendirat(struct wamb *wamb, dev_t dev, ino_t ino)
{
	struct wambdir *dir = wambdir_read(wamb, dev, ino);
	if(dir == NULL) {
		return NULL;
	}

	qsort(dir->ent_list, dir->ent_count, sizeof(struct wambent), fn_comp_ent);

	return dir;
}


struct wambent *wamb_finddir(wambdir *dir, const char *name)
{
	size_t i;
	struct wambent *ent = dir->ent_list;

	for(i=0; i<dir->ent_count; i++) {
		if(strcmp(name, ent->name) == 0) {
			return ent;
		}
		ent++;
	}

	return NULL;
}


wambdir *wamb_opendir(struct wamb *wamb, const char *path)
{
	/* Canonicalized path */

	char *path_canon = realpath(path, NULL);
	if(path_canon == NULL) {
		fprintf(stderr, "Error converting path %s: %s\n", path, strerror(errno));
		return NULL;
	}

	/* Find top path in database */

	int l = strlen(path_canon);
	dev_t dev;
	ino_t ino;
	while(l > 0) {
		size_t vallen;
		char *val = db_get(wamb->db, path_canon, l, &vallen);
		if(val) {
			sscanf(val, "%jd %jd", &dev, &ino);
			free(val);
			break;
		}
		l--;
		while(l > 1  && path_canon[l] != '/') l--;
	}

	if(l == 0) {
		fprintf(stderr, "Path %s not found in database\n", path_canon);
		free(path_canon);
		return NULL;
	}

	struct wambdir *dir;

	dir = wamb_opendirat(wamb, dev, ino);
	if(dir == NULL) {
		return NULL;
	}
	
	char rest[256];
	strncpy(rest, path_canon+l, sizeof rest);

	char *save;
	char *name = strtok_r(rest, "/", &save);

	while(dir && name) {

		struct wambent *ent = wamb_finddir(dir, name);

		struct wambdir *dir_next = NULL;

		if(ent) {
			dir_next = wamb_opendirat(wamb, ent->dev, ent->ino);
		}

		wamb_closedir(dir);
		dir = dir_next;
		name = strtok_r(NULL, "/", &save);
	}

	return dir;
}


struct wambent *wamb_readdir(wambdir *dir)
{
	if(dir->ent_cur < dir->ent_count) {
		struct wambent *ent = &dir->ent_list[dir->ent_cur];
		dir->ent_cur ++;
		return ent;
	} else {
		return NULL;
	}
}


int wamb_rewinddir(wambdir *dir)
{
	dir->ent_cur = 0;
	return 0;
}


void wamb_closedir(wambdir *dir)
{
}

/*
 * End
 */

