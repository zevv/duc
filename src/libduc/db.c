
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

	struct buffer *b = buffer_new(NULL, 0);

	buffer_put_index_report(b, report);
	db_put(duc->db, report->path, strlen(report->path), b->data, b->len);
	buffer_free(b);

	return 0;
}


struct duc_index_report *db_read_report(duc *duc, const char *path)
{
	struct duc_index_report *report;
	size_t vall;

	char *val = db_get(duc->db, path, strlen(path), &vall);
	if(val == NULL) {
		duc->err = DUC_E_PATH_NOT_FOUND;
		return NULL;
	}

	struct buffer *b = buffer_new(val, vall);

	report = duc_malloc(sizeof *report);
	buffer_get_index_report(b, report);
	buffer_free(b);

	return report;
}


/*
 * End
 */

