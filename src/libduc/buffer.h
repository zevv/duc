#ifndef buffer_h
#define buffer_h

struct buffer {
	void *data;
	size_t max;
	size_t len;
	size_t ptr;
};

struct buffer *buffer_new(void *data, size_t len);
void buffer_free(struct buffer *b);

void buffer_dump(struct buffer *b);
void buffer_seek(struct buffer *b, size_t off);

int buffer_put(struct buffer *b, const void *data, size_t len);
int buffer_put_varint(struct buffer *b, uint64_t v);
int buffer_put_string(struct buffer *b, const char *s);

int buffer_get(struct buffer *b, void *data, size_t len);
int buffer_get_string(struct buffer *b, char **s);
int buffer_get_varint(struct buffer *b, uint64_t *v);

void buffer_put_dir(struct buffer *b, struct duc_devino *devino);
void buffer_get_dir(struct buffer *b, struct duc_devino *devino);

void buffer_put_dirent(struct buffer *b, struct duc_dirent *ent);
void buffer_get_dirent(struct buffer *b, struct duc_dirent *ent);

void buffer_put_index_report(struct buffer *b, struct duc_index_report *report);
void buffer_get_index_report(struct buffer *b, struct duc_index_report *report);

#endif
