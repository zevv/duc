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

		/* write topn array: flatten pointer array into contiguous struct data */
		if (report->topn_cnt > 0) {
			char str[] = "duc_index_topn_info";
			int str_len = sizeof(str);
			size_t topn_size = (size_t)report->topn_cnt * sizeof(duc_topn_file);
			char *topn_flat = duc_malloc(topn_size);
			for (int i = 0; i < report->topn_cnt; i++)
				memcpy(topn_flat + (size_t)i * sizeof(duc_topn_file), report->topn_array[i], sizeof(duc_topn_file));
			char *topn_prev = db_get(duc->db, str, str_len, &tmpl);
			if (topn_prev) {
				topn_prev = duc_realloc(topn_prev, tmpl + topn_size);
				memcpy(topn_prev + tmpl, topn_flat, topn_size);
				db_put(duc->db, str, str_len, topn_prev, tmpl + topn_size);
				duc_free(topn_prev);
			} else {
				db_put(duc->db, str, str_len, topn_flat, topn_size);
			}
			duc_free(topn_flat);
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
	
	char kyotocabinet[] = { 0x4b,0x43,0x0a,0x0,0x10,0x0e,0x06,0xb4,0x31,0x08,0x0a,0x04,0x00,0x00,0x00,0x00 };
	if (memcmp(buf,kyotocabinet,16) == 0) {
	    return("kyotocabinet");
	}
	
	if (strncmp(buf,"ToKyO CaBiNeT",13) == 0) {
	    return("tokyocabinet");
	}

	if (strncmp(buf,"TkrzwHDB",8) == 0) {
	    return("tkrzw");
	}

	if (strncmp(buf,"SQLite format 3",15) == 0) {
	    return("sqlite3");
	}
	
	char lmdb[] = { 0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x08,0x0,0x0,0x0,0x0,0x0,
                        0xde,0xc0,0xef,0xbe,0x01,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0 };
	if (memcmp(buf,lmdb,32) == 0) {
	    return("lmdb");
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

