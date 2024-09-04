#include "config.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "duc.h"
#include "db.h"
#include "buffer.h"
#include "private.h"

#define MAGIC_LEN 64



/* 
 * Store report. Add the report index to the 'duc_index_reports' key if not
 * previously indexed 
 */

duc_errno db_write_report(duc *duc, const struct duc_index_report *report)
{
	size_t tmpl;
	char *tmp = db_get(duc->db, report->path, strlen(report->path), &tmpl);

	//printf("writing report, ->topn_cnt = %d, ->topn_cnt_max = %d\n",report->topn_cnt, report->topn_cnt_max);
	if(tmp == NULL) {
		char *tmp = db_get(duc->db, "duc_index_reports", 17, &tmpl);
		if(tmp) {
			tmp = duc_realloc(tmp, tmpl + sizeof(report->path));
			memcpy(tmp + tmpl, report->path, sizeof(report->path));
			db_put(duc->db, "duc_index_reports", 17, tmp, tmpl + sizeof(report->path));
		} else {
			db_put(duc->db, "duc_index_reports", 17, report->path, sizeof(report->path));
		}
		
		/* write histogram */
		tmp = db_get(duc->db, "duc_index_histograms", 20, &tmpl);
		if (tmp) {
			tmp = duc_realloc(tmp, tmpl + sizeof(report->histogram));
			memcpy(tmp + tmpl, report->histogram, sizeof(report->histogram));
			db_put(duc->db, "duc_index_histograms", 20, tmp, 
			       tmpl + sizeof(report->histogram));
		} else {
			db_put(duc->db, "duc_index_histograms", 20, report->histogram, 
			       sizeof(report->histogram));
		}

		/* write topn array, FIXME to really work... */
		char str[] = "duc_index_topn_info";
		int str_len = sizeof(str);
		tmp = db_get(duc->db, str, str_len , &tmpl);
		if (tmp) {
			tmp = duc_realloc(tmp, tmpl + sizeof(report->topn_array));
			memcpy(tmp + tmpl, report->topn_array, sizeof(report->topn_array));
			db_put(duc->db, str, str_len, tmp, 
			       tmpl + sizeof(report->topn_array));
		} else {
			db_put(duc->db, str, str_len, report->topn_array, 
			       sizeof(report->topn_array));
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

/* Return what type of DB we think this is.  Note, leveldb is a directory... */
   
char *duc_db_type_check(const char *path_db)
{
    struct stat sb;

    stat(path_db,&sb);

    if (S_ISREG(sb.st_mode)) {

	FILE *f = fopen(path_db,"r");

	if(f == NULL) {
	    //duc_log(NULL, DUC_LOG_DBG, "Not reading configuration from '%s': %s", path, streo;
	    return("unknown");
	}
	
	char buf[MAGIC_LEN];
		
	/* read first MAGIC_LEN bytes of file then look for the strings, etc for each type of DB we support. */
	size_t len = fread(buf, 1, sizeof(buf),f);
	
	if (strncmp(buf,"Kyoto CaBiNeT",13) == 0) {
	    return("Kyoto Cabinet");
	}
	
	if (strncmp(buf,"ToKyO CaBiNeT",13) == 0) {
	    return("Tokyo Cabinet");
	}

	if (strncmp(buf,"TkrzwHDB",8) == 0) {
	    return("Tkrzw HashDBM");
	}

	if (strncmp(buf,"SQLite format 3",15) == 0) {
	    return("SQLite3");
	}
	
    }

    /* Check for DB_PATH that's a directory, and look in there. */
    if (S_ISDIR(sb.st_mode)) {
	return("leveldb");
    }
    return("unknown");
}

/*
 * End
 */

