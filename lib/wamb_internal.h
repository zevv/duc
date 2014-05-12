#ifndef wamb_internal_h
#define wamb_internal_h

struct wamb {
	struct db *db;
};


struct wamb_child {
	char name[256];
	off_t size;
	dev_t dev;
	ino_t ino;
};


struct wamb_node {
	dev_t dev;
	ino_t ino;
	struct wamb_child *child_list;
	size_t child_count;
	size_t child_max;
};


struct wambdir *wambdir_new(struct wamb *wamb, size_t ent_max);
void wambdir_add_ent(struct wambdir *dir, const char *name, size_t size, dev_t dev, ino_t ino);

struct wamb_node *wamb_node_new(dev_t dev, ino_t ino);
void wamb_node_free(struct wamb_node *node);

void wamb_node_add_child(struct wamb_node *node, const char *name, off_t size, dev_t dev, ino_t ino);

int wamb_node_write(struct wamb *wamb, struct wamb_node *node);
struct wamb_node *wamb_find_dir(struct wamb *wamb, const char *path);

int wamb_root_write(struct wamb *wamb, const char *path, dev_t dev, ino_t ino);

#endif

