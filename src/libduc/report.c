
#include "config.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "duc.h"
#include "db.h"
#include "buffer.h"
#include "private.h"


/* 
 * Store report. Add the report index to the 'duc_index_reports' key if not
 * previously indexed 
 */

duc_errno report_write(duc *duc, struct duc_index_report *report)
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

	struct buffer *b = buffer_new(NULL, 0);
	buffer_put_string(b, report->path);
	buffer_put_varint(b, report->devino.dev);
	buffer_put_varint(b, report->devino.ino);
	buffer_put_varint(b, report->time_start.tv_sec);
	buffer_put_varint(b, report->time_start.tv_usec);
	buffer_put_varint(b, report->time_stop.tv_sec);
	buffer_put_varint(b, report->time_stop.tv_usec);
	buffer_put_varint(b, report->file_count);
	buffer_put_varint(b, report->dir_count);
	buffer_put_varint(b, report->size.apparent);
	buffer_put_varint(b, report->size.actual);

	db_put(duc->db, report->path, strlen(report->path), b->data, b->len);

	buffer_free(b);

	return 0;
}


struct duc_index_report *report_read(duc *duc, const char *path)
{
	struct duc_index_report *rep;
	size_t vall;
	char *vs;
	uint64_t vi;

	char *val = db_get(duc->db, path, strlen(path), &vall);
	if(val == NULL) {
		duc->err = DUC_E_PATH_NOT_FOUND;
		return NULL;
	}


	rep = duc_malloc(sizeof *rep);

	struct buffer *b = buffer_new(val, vall);

	buffer_get_string(b, &vs);
	snprintf(rep->path, sizeof(rep->path), "%s", vs);
	free(vs);

	buffer_get_varint(b, &vi); rep->devino.dev = vi;
	buffer_get_varint(b, &vi); rep->devino.ino = vi;
	buffer_get_varint(b, &vi); rep->time_start.tv_sec = vi;
	buffer_get_varint(b, &vi); rep->time_start.tv_usec = vi;
	buffer_get_varint(b, &vi); rep->time_stop.tv_sec = vi;
	buffer_get_varint(b, &vi); rep->time_stop.tv_usec = vi;
	buffer_get_varint(b, &vi); rep->file_count = vi;
	buffer_get_varint(b, &vi); rep->dir_count = vi;
	buffer_get_varint(b, &vi); rep->size.apparent = vi;
	buffer_get_varint(b, &vi); rep->size.actual = vi;

	buffer_free(b);

	return rep;
}


/*
 * End
 */

