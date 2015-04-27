
#include "config.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "duc.h"
#include "db.h"
#include "buffer.h"
#include "private.h"



static int mkkey(struct duc_devino *devino, char *key, size_t keylen)
{
	return snprintf(key, keylen, "%jx/%jx", (uintmax_t)devino->dev, (uintmax_t)devino->ino);
}


/*
 * Serialize duc_dir into a database record
 */

duc_errno db_write_dir(struct duc_dir *dir)
{
	struct buffer *b = buffer_new(NULL, 0);
		
	buffer_put_varint(b, dir->devino_parent.dev);
	buffer_put_varint(b, dir->devino_parent.ino);

	int i;
	struct duc_dirent *ent = dir->ent_list;

	for(i=0; i<dir->ent_count; i++) {
		buffer_put_string(b, ent->name);
		buffer_put_varint(b, ent->size.apparent);
		buffer_put_varint(b, ent->size.actual);
		buffer_put_varint(b, ent->type);
		if(ent->type == DT_DIR) {
			buffer_put_varint(b, ent->devino.dev);
			buffer_put_varint(b, ent->devino.ino);
		}
		ent++;
	}

	char key[32];
	size_t keyl = mkkey(&dir->devino, key, sizeof key);
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

struct duc_dir *db_read_dir(struct duc *duc, struct duc_devino *devino)
{
	struct duc_dir *dir = duc_dir_new(duc, devino);

	char key[32];
	size_t keyl;
	size_t vall;

	keyl = mkkey(devino, key, sizeof key);
	char *val = db_get(duc->db, key, keyl, &vall);
	if(val == NULL) {
		duc_log(duc, DUC_LOG_WRN, "Key %s not found in database", key);
		duc->err = DUC_E_PATH_NOT_FOUND;
		return NULL;
	}

	struct buffer *b = buffer_new(val, vall);

	struct duc_size size_total = { 0, 0 };

	uint64_t v;
	buffer_get_varint(b, &v); dir->devino_parent.dev = v;
	buffer_get_varint(b, &v); dir->devino_parent.ino = v;
	
	while(b->ptr < b->len) {

		uint64_t size_apparent;
		uint64_t size_actual;
		uint64_t dev;
		uint64_t ino;
		uint64_t type;
		char *name;

		buffer_get_string(b, &name);
		buffer_get_varint(b, &size_apparent);
		buffer_get_varint(b, &size_actual);
		buffer_get_varint(b, &type);

		if(type == DT_DIR) {
			buffer_get_varint(b, &dev);
			buffer_get_varint(b, &ino);
			dir->dir_count ++;
		} else {
			dir->file_count ++;
		}

		if(name) {
			struct duc_size size = {
				.apparent = size_apparent,
				.actual = size_actual,
			};
			struct duc_devino devino = {
				.dev = dev,
				.ino = ino,
			};
			duc_dir_add_ent(dir, name, &size, type, &devino);
			free(name);
		}

		size_total.apparent += size_apparent;
		size_total.actual += size_actual;
	
	}

	dir->size = size_total;

	buffer_free(b);

	return dir;
}



/* 
 * Store report. Add the report index to the 'duc_index_reports' key if not
 * previously indexed 
 */

duc_errno db_write_report(duc *duc, struct duc_index_report *report)
{

	size_t tmpl;
	char *tmp = db_get(duc->db, report->path, strlen(report->path), &tmpl);

	if(tmp == NULL) {
		char *tmp = db_get(duc->db, "duc_index_reports", 17, &tmpl);
		if(tmp) {
			tmp = realloc(tmp, tmpl + sizeof(report->path));
			memcpy(tmp + tmpl, report->path, sizeof(report->path));
			db_put(duc->db, "duc_index_reports", 17, tmp, tmpl + sizeof(report->path));
		} else {
			db_put(duc->db, "duc_index_reports", 17, report->path, sizeof(report->path));
		}
	} else {
		free(tmp);
	}

	db_put(duc->db, report->path, strlen(report->path), report, sizeof *report);

	return 0;
}

/*
 * End
 */

