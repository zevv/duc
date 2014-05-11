
#ifndef wamb_h
#define wamb_h

enum {
	WAMB_INDEX_XDEV	= 1<<0,
};

int wamb_index(const char *path_db, const char *path, int flags);


typedef struct {
	dev_t dev;
	ino_t ino;
} wamb_uid;

typedef struct wamb_iter *wamb_iter;

struct wamb_ent {
	char name[NAME_MAX];
	off_t size;
	mode_t mode;
	wamb_uid uid;
};

wamb_iter wamb_list(const char *path_db);
void wamb_iter_read(wamb_iter iter);
void wamb_iter_seek(wamb_iter iter, long offset);
void wamb_iter_close(wamb_iter iter);

#endif
