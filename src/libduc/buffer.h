#ifndef buffer_h
#define buffer_h

/* err is set by the first read past len; later reads then fail as well */

struct buffer {
	uint8_t *data;
	size_t max;
	size_t len;
	size_t ptr;
	int err;
};

struct buffer *buffer_new(void *data, size_t len);
void buffer_free(struct buffer *b);

void buffer_put_dir(struct buffer *b, const struct duc_devino *devino, time_t mtime);
void buffer_get_dir(struct buffer *b, struct duc_devino *devino, time_t *mtime);

void buffer_put_dirent(struct buffer *b, const struct duc_dirent *ent);
void buffer_get_dirent(struct buffer *b, struct duc_dirent *ent);

void buffer_put_index_report(struct buffer *b, const struct duc_index_report *report);
int buffer_get_index_report(struct buffer *b, struct duc_index_report *report);

#endif
