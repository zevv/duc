
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <tcutil.h>
#include <tchdb.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h> 

#include "db.h"

struct db {
	TCHDB* hdb;
};


struct db *db_open(const char *path_db, const char *mode)
{
	struct db *db;
	char tmp[PATH_MAX];

	if(path_db == NULL) {
		path_db = getenv("PS_PATH_DB");
	}

	if(path_db == NULL) {
		char *home = getenv("HOME");
		if(home) {
			snprintf(tmp, sizeof tmp, "%s/.wamb.db", home);
			path_db = tmp;
		}
	}

	db = malloc(sizeof *db);
	assert(db);

	db->hdb = tchdbnew();

	uint32_t flags = 0;
	if(strchr(mode, 'r')) flags |= HDBOREADER;
	if(strchr(mode, 'w')) flags |= HDBOWRITER;
	if(strchr(mode, 'c')) flags |= HDBOCREAT;

	tchdbopen(db->hdb, path_db, flags);

	return db;
}


void db_close(struct db *db)
{
	tchdbclose(db->hdb);
	free(db);
}


int db_root_write(struct db *db, const char *path, dev_t dev, ino_t ino)
{
	char key[PATH_MAX];
	char val[32];
	snprintf(key, sizeof key, "%s", path);
	snprintf(val, sizeof val, "%jd %jd", dev, ino);
	tchdbput(db->hdb, key, strlen(key), val, strlen(val));
	return 0;
}


struct db_node *db_node_new(dev_t dev, ino_t ino)
{
	struct db_node *node;

	node = malloc(sizeof(struct db_node));
	assert(node);
	node->child_count = 0;
	node->child_max = 32;
	node->dev = dev;
	node->ino = ino;

	node->child_list = malloc(sizeof(struct db_child) * node->child_max);
	assert(node->child_list);

	return node;
}


void db_node_add_child(struct db_node *node, const char *name, off_t size, dev_t dev, ino_t ino)
{
	if(node->child_count >= node->child_max) {
		node->child_max *= 2;
		node->child_list = realloc(node->child_list, sizeof(struct db_child) * node->child_max);
		assert(node->child_list);
	}

	struct db_child *child = &node->child_list[node->child_count];
	node->child_count ++;

	strncpy(child->name, name, sizeof(child->name));
	child->size = size;
	child->dev = dev;
	child->ino = ino;
}


struct db_node *db_read_node(struct db *db, dev_t dev, ino_t ino)
{
	char key[32];
	size_t keyl;
	char *val;
	int vall;

	keyl = snprintf(key, sizeof key, "d %jd %jd", dev, ino);
	val = tchdbget(db->hdb, key, keyl, &vall);
	if(val == NULL) return NULL;

	struct db_node *node = db_node_new(dev, ino);
	node->child_list = (void *)val;
	node->child_count = vall / sizeof(struct db_child);

	return node;
}


int db_node_write(struct db *db, struct db_node *node)
{
	char key[32];
	snprintf(key, sizeof key, "d %jd %jd", node->dev, node->ino);
	tchdbput(db->hdb, key, strlen(key), (void *)node->child_list, node->child_count * sizeof(struct db_child));
	return 0;
}


struct db_node *db_find_child(struct db *db, struct db_node *node, const char *name)
{
	size_t i;

	for(i=0; i<node->child_count; i++) {
		struct db_child *child = &node->child_list[i];
		if(strcmp(child->name, name) == 0) {
			return db_read_node(db, child->dev, child->ino);
		}
	}
	return NULL;
}


struct db_node *db_find_dir(struct db *db, const char *path)
{
	int l = strlen(path);
	char *val;
	int vallen;
	dev_t dev;
	ino_t ino;
	
	/* Find top path in database */

	while(l > 0) {
		val = tchdbget(db->hdb, path, l, &vallen);
		if(val) {
			sscanf(val, "%jd %jd", &dev, &ino);
			free(val);
			break;
		}
		l--;
		while(l> 0  && path[l] != '/') l--;
	}

	if(l == 0) return NULL;

	/* Find rest of path by iterating the database */

	struct db_node *node = db_read_node(db, dev, ino);
	
	char buf[PATH_MAX+1];
	strncpy(buf, path+l, sizeof buf);

	char *save;
	char *name = strtok_r(buf, "/", &save);

	while(node && name) {
		struct db_node *node_next = db_find_child(db, node, name);
		db_node_free(node);
		node = node_next;
		name = strtok_r(NULL, "/", &save);
	}

	return node;

}


void db_node_free(struct db_node *node)
{
	free(node);
}


/*
 * End
 */
