#ifndef wamb_internal_h
#define wamb_internal_h

struct wamb {
	struct db *db;
};


struct wambdir *wambdir_new(struct wamb *wamb, size_t ent_max);
void wambdir_add_ent(struct wambdir *dir, const char *name, size_t size, dev_t dev, ino_t ino);

int wambdir_write(struct wambdir *dir, dev_t dev, ino_t ino);
struct wambdir *wambdir_read(struct wamb *wamb, dev_t dev, ino_t ino);

int wamb_root_write(struct wamb *wamb, const char *path, dev_t dev, ino_t ino);

#endif

