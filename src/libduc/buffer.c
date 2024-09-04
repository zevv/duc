
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#include "private.h"
#include "buffer.h"
#include "varint.h"


/*
 * Generic buffer manipulation for serialization
 */


struct buffer *buffer_new(void *data, size_t len)
{
	struct buffer *b;

	b = duc_malloc(sizeof(struct buffer));
	b->ptr = 0;

	if(data) {
		b->max = len;
		b->len = len;
		b->data = data;
	} else {
		b->max = 1024;
		b->len = 0;
		b->data = duc_malloc(b->max);
	}

	return b;
}


void buffer_free(struct buffer *b)
{
	duc_free(b->data);
	duc_free(b);
}

// Add item to buffer, but grow by doubling if needed
static int buffer_put(struct buffer *b, const void *data, size_t len)
{
	if(b->ptr + len > b->max) {
		while(b->len + len > b->max) {
			b->max *= 2;
		}
		b->data = duc_realloc(b->data, b->max);
	}

	memcpy(b->data + b->ptr, data, len);
	b->ptr += len;
	if(b->ptr > b->len) b->len = b->ptr;
	return len;
}


static int buffer_get(struct buffer *b, void *data, size_t len)
{
	if(b->ptr <= b->len - len) {
		memcpy(data, b->data + b->ptr, len);
		b->ptr += len;
		return len;
	} else {
		return 0;
	}
}


static int buffer_put_varint(struct buffer *b, uint64_t v)
{
	uint8_t buf[9];
	int l = PutVarint64(buf, v);
	buffer_put(b, buf, l);
	return l;
} 


static int buffer_get_varint(struct buffer *b, uint64_t *v)
{
	uint8_t buf[9];
	int r = buffer_get(b, buf, 1);
	if(r == 0) return 0;

	int n = 0;
	if(buf[0] >= 249) {
		n = buf[0] - 247;
	} else if(buf[0] >= 241) {
		n = 1;
	}
	if(n > 0) buffer_get(b, buf+1, n);
	int l = GetVarint64(buf, n+1, v);
	return l;
}


static void buffer_put_string(struct buffer *b, const char *s)
{
	size_t len = strlen(s);
	if(len < 256) {
		uint8_t l = len;
		buffer_put(b, &l, sizeof l);
		buffer_put(b, s, l);
	}
	else {
	    fprintf(stderr,"cannot buffer_put_string() larger than 255 bytes\n");
	    exit(1);
	}
}

// FIXME!  This must return something on error.  Or crash cleanly?
// Maximum string size of 255 bytes... why?
static void buffer_get_string(struct buffer *b, char **sout)
{
	uint8_t len;
	buffer_get(b, &len, sizeof(len));
	char *s = duc_malloc(len + 1);
	if(s) {
		buffer_get(b, s, len);
		s[len] = '\0';
		*sout = s;
	}
}


static void buffer_put_devino(struct buffer *b, const struct duc_devino *devino)
{
	buffer_put_varint(b, devino->dev);
	buffer_put_varint(b, devino->ino);
}


static void buffer_get_devino(struct buffer *b, struct duc_devino *devino)
{
	uint64_t v;
	buffer_get_varint(b, &v); devino->dev = v;
	buffer_get_varint(b, &v); devino->ino = v;
}


static void buffer_put_size(struct buffer *b, const struct duc_size *size)
{
	buffer_put_varint(b, size->apparent);
	buffer_put_varint(b, size->actual);
	buffer_put_varint(b, size->count);
}


static void buffer_get_size(struct buffer *b, struct duc_size *size)
{
	uint64_t v;
	buffer_get_varint(b, &v); size->apparent = v;
	buffer_get_varint(b, &v); size->actual = v;
	buffer_get_varint(b, &v); size->count = v;
}


/*
 * Serialize data from structs into buffer
 */

void buffer_put_dir(struct buffer *b, const struct duc_devino *devino, time_t mtime)
{
	buffer_put_devino(b, devino);
	buffer_put_varint(b, mtime);
}


void buffer_get_dir(struct buffer *b, struct duc_devino *devino, time_t *mtime)
{
	uint64_t v;
	buffer_get_devino(b, devino);
	buffer_get_varint(b, &v); *mtime = v;
}

void buffer_put_dirent(struct buffer *b, const struct duc_dirent *ent)
{
	buffer_put_string(b, ent->name);
	buffer_put_size(b, &ent->size);
	buffer_put_varint(b, ent->type);

	if(ent->type == DUC_FILE_TYPE_DIR) {
		buffer_put_devino(b, &ent->devino);
	}
}

void buffer_get_dirent(struct buffer *b, struct duc_dirent *ent)
{
	uint64_t v;

	buffer_get_string(b, &ent->name);
	buffer_get_size(b, &ent->size);
	buffer_get_varint(b, &v); ent->type = v;
	
	if(ent->type == DUC_FILE_TYPE_DIR) {
		buffer_get_devino(b, &ent->devino);
	}
}


/* make sure these next two are in sync, the format needs to be identical */
void buffer_put_index_report(struct buffer *b, const struct duc_index_report *report)
{
	buffer_put_string(b, report->path);
	buffer_put_devino(b, &report->devino);
	buffer_put_varint(b, report->time_start.tv_sec);
	buffer_put_varint(b, report->time_start.tv_usec);
	buffer_put_varint(b, report->time_stop.tv_sec);
	buffer_put_varint(b, report->time_stop.tv_usec);
	buffer_put_varint(b, report->file_count);
	buffer_put_varint(b, report->dir_count);
	buffer_put_size(b, &report->size);
	/* write topN data */
	buffer_put_varint(b, report->topn_min_size);
	buffer_put_varint(b, report->topn_cnt);
	buffer_put_varint(b, report->topn_cnt_max);
	buffer_put_varint(b, report->histogram_buckets);

	/* Make this dynamic where the last bucket has -1 maybe */
	for(int i = 0; i < report->histogram_buckets; i++) {
	    buffer_put_varint(b,report->histogram[i]);
	}

	/* write topN data, FIXME */
	for(int i = 0; i < report->topn_cnt; i++) {
	    buffer_put_varint(b, strlen(report->topn_array[i]->name));
	    buffer_put_string(b, report->topn_array[i]->name);
	    buffer_put_varint(b, report->topn_array[i]->size);
	}
}

/* must have identical layout as buffer_put_index_report()! */
void buffer_get_index_report(struct buffer *b, struct duc_index_report *report)
{
	char *vs = NULL;
	buffer_get_string(b, &vs);
	if(vs == NULL) return;

	snprintf(report->path, sizeof(report->path), "%s", vs);
	duc_free(vs);

	uint64_t vi;
	buffer_get_devino(b, &report->devino);
	buffer_get_varint(b, &vi); report->time_start.tv_sec = vi;
	buffer_get_varint(b, &vi); report->time_start.tv_usec = vi;
	buffer_get_varint(b, &vi); report->time_stop.tv_sec = vi;
	buffer_get_varint(b, &vi); report->time_stop.tv_usec = vi;
	buffer_get_varint(b, &vi); report->file_count = vi;
	buffer_get_varint(b, &vi); report->dir_count = vi;
	buffer_get_size(b, &report->size);
        /* read topN data as well, if found */
	buffer_get_varint(b, &vi); report->topn_min_size = vi;
	buffer_get_varint(b, &vi); report->topn_cnt = vi;
	buffer_get_varint(b, &vi); report->topn_cnt_max = vi;
	buffer_get_varint(b, &vi); report->histogram_buckets = vi;

	/* when reading, look for -1 as last bucket, so we can be dynamically sized? */
	for(int i = 0; i < report->histogram_buckets; i++) {
	    buffer_get_varint(b, &vi);
	    report->histogram[i] = vi;
	}

	for(int i = 0; i < report->topn_cnt; i++) {
	    uint64_t length;
	    buffer_get_varint(b, &length);
	    buffer_get_string(b, &vs);
	    report->topn_array[i] = duc_malloc0(sizeof(duc_topn_file));
	    strncpy(report->topn_array[i]->name, vs, strlen(vs));
	    buffer_get_varint(b, &vi); report->topn_array[i]->size = vi;
	}
}


/*
 * End
 */

