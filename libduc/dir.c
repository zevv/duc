
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


struct ducdir {
	struct duc *duc;
	struct ducent *ent_list;
	mode_t mode;
	size_t ent_cur;
	size_t ent_count;
	size_t ent_max;
};


struct ducdir *ducdir_new(struct duc *duc, size_t ent_max)
{
	struct ducdir *dir = malloc(sizeof(struct ducdir));

	if(dir == NULL) {
		return NULL;
	}

	dir->duc = duc;
	dir->ent_cur = 0;
	dir->ent_count = 0;
	dir->ent_max = ent_max;
	dir->ent_list = malloc(sizeof(struct ducent) * ent_max);

	if(dir->ent_list == NULL) {
		free(dir);
		return NULL;
	}

	return dir;
}


void ducdir_add_ent(struct ducdir *dir, const char *name, size_t size, mode_t mode, dev_t dev, ino_t ino)
{
	if(dir->ent_count >= dir->ent_max) {
		dir->ent_max *= 2;
		dir->ent_list = realloc(dir->ent_list, sizeof(struct ducent) * dir->ent_max);
		assert(dir->ent_list);
	}

	struct ducent *ent = &dir->ent_list[dir->ent_count];
	dir->ent_count ++;

	strncpy(ent->name, name, sizeof(ent->name));
	ent->size = size;
	ent->mode = mode;
	ent->dev = dev;
	ent->ino = ino;
}


static int fn_comp_ent(const void *a, const void *b)
{
	const struct ducent *ea = a;
	const struct ducent *eb = b;
	return(ea->size < eb->size);
}


static int mkkey(dev_t dev, ino_t ino, char *key, size_t keylen)
{
	return snprintf(key, keylen, "%jd/%jd", dev, ino);
}


/*
 * Serialize ducdir into a database record
 */

int ducdir_write(struct ducdir *dir, dev_t dev, ino_t ino)
{
	struct buffer *b = buffer_new(NULL, 0);

	int i;
	struct ducent *ent = dir->ent_list;

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
	db_put(dir->duc->db, key, keyl, b->data, b->len);

	buffer_free(b);

	return 0;
}


/*
 * Read database record and deserialize into ducdir
 */

struct ducdir *ducdir_read(struct duc *duc, dev_t dev, ino_t ino)
{
	struct ducdir *dir = ducdir_new(duc, 8);
	if(dir == NULL) {
		return NULL;
	}

	char key[32];
	size_t keyl;
	size_t vall;

	keyl = mkkey(dev, ino, key, sizeof key);
	char *val = db_get(duc->db, key, keyl, &vall);
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
		
		ducdir_add_ent(dir, name, size, mode, dev, ino);
	}

	return dir;
}


ducdir *duc_opendirat(struct duc *duc, dev_t dev, ino_t ino)
{
	struct ducdir *dir = ducdir_read(duc, dev, ino);
	if(dir == NULL) {
		return NULL;
	}

	qsort(dir->ent_list, dir->ent_count, sizeof(struct ducent), fn_comp_ent);

	return dir;
}


struct ducent *duc_finddir(ducdir *dir, const char *name)
{
	size_t i;
	struct ducent *ent = dir->ent_list;

	for(i=0; i<dir->ent_count; i++) {
		if(strcmp(name, ent->name) == 0) {
			return ent;
		}
		ent++;
	}

	return NULL;
}


ducdir *duc_opendir(struct duc *duc, const char *path)
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
		char *val = db_get(duc->db, path_canon, l, &vallen);
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

	struct ducdir *dir;

	dir = duc_opendirat(duc, dev, ino);
	if(dir == NULL) {
		return NULL;
	}
	
	char rest[256];
	strncpy(rest, path_canon+l, sizeof rest);

	char *save;
	char *name = strtok_r(rest, "/", &save);

	while(dir && name) {

		struct ducent *ent = duc_finddir(dir, name);

		struct ducdir *dir_next = NULL;

		if(ent) {
			dir_next = duc_opendirat(duc, ent->dev, ent->ino);
		}

		duc_closedir(dir);
		dir = dir_next;
		name = strtok_r(NULL, "/", &save);
	}

	return dir;
}


struct ducent *duc_readdir(ducdir *dir)
{
	if(dir->ent_cur < dir->ent_count) {
		struct ducent *ent = &dir->ent_list[dir->ent_cur];
		dir->ent_cur ++;
		return ent;
	} else {
		return NULL;
	}
}


int duc_rewinddir(ducdir *dir)
{
	dir->ent_cur = 0;
	return 0;
}


void duc_closedir(ducdir *dir)
{
}

/*
 * End
 */

