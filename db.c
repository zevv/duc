
#include <kclangc.h>
#include <bsd/string.h>
#include <stdlib.h>

#include "db.h"


struct db *db_open(const char *mode)
{
	struct db *db;

	db = malloc(sizeof *db);
	assert(db);

	db->kcdb = kcdbnew();

	uint32_t flags = 0;
	if(strchr(mode, 'r')) flags |= KCOREADER;
	if(strchr(mode, 'w')) flags |= KCOWRITER;
	if(strchr(mode, 'c')) flags |= KCOCREATE;

	kcdbopen(db->kcdb, "files.kch#opts=", flags);

	return db;
}


void db_close(struct db *db)
{
	kcdbclose(db->kcdb);
	free(db);
}


static int fn_comp_child(const void *a, const void *b)
{
	const struct db_child *da = a;
	const struct db_child *db = b;
	return(da->size < db->size);
}


struct db_node *db_read_node(struct db *db, dev_t dev, ino_t ino)
{
	char key[32];
	size_t keyl;
	char *val;
	size_t vall;

	keyl = snprintf(key, sizeof key, "d %jd %jd", dev, ino);
	val = kcdbget(db->kcdb, key, keyl, &vall);
	if(val == NULL) return NULL;

	struct db_node *node = (void *)val;
	node = malloc(sizeof(struct db_node));
	assert(node);
	node->dev = dev;
	node->ino = ino;
	node->child_list = (void *)val;
	node->child_count = vall / sizeof(struct db_child);

	qsort(node->child_list, node->child_count, sizeof(struct db_child), fn_comp_child);

	kcfree(val);
	return node;
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
	size_t vallen;
	dev_t dev;
	ino_t ino;
	
	/* Find top path in database */

	while(l > 0) {
		val = kcdbget(db->kcdb, path, l, &vallen);
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
	
	char buf[PATH_MAX];
	strlcpy(buf, path+l, sizeof buf);

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
