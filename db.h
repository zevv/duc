#ifndef db_h
#define db_h

#include <kclangc.h>

struct db {
	KCDB* kcdb;
};


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
};


struct db *db_open(const char *mode);
void db_close(struct db *db);

struct db_node *db_find_dir(struct db *db, const char *path);

void db_node_free(struct db_node *node);


#endif
