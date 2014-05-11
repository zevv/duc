#ifndef db_h
#define db_h

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


struct db;

struct db_child {
	char name[NAME_MAX];
	off_t size;
	dev_t dev;
	ino_t ino;
};


struct db_node {
	dev_t dev;
	ino_t ino;
	struct db_child *child_list;
	size_t child_count;
	size_t child_max;
};


struct db *db_open(const char *mode);
void db_close(struct db *db);

struct db_node *db_node_new(dev_t dev, ino_t ino);
void db_node_free(struct db_node *node);

void db_node_add_child(struct db_node *node, const char *name, off_t size, dev_t dev, ino_t ino);

int db_node_write(struct db *db, struct db_node *node);
struct db_node *db_find_dir(struct db *db, const char *path);

int db_root_write(struct db *db, const char *path, dev_t dev, ino_t ino);

#endif
