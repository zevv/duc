
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "wamb.h"
#include "wamb_internal.h"
#include "db.h"


struct wamb *wamb_open(const char *path_db, int flags)
{
	char tmp[PATH_MAX];
	
	struct wamb *wamb = malloc(sizeof *wamb);
	if(wamb == NULL) return NULL;

	if(path_db == NULL) {
		path_db = getenv("WAMB_DATABASE");
	}

	if(path_db == NULL) {
		char *home = getenv("HOME");
		if(home) {
			snprintf(tmp, sizeof tmp, "%s/.wamb.db", home);
			path_db = tmp;
		}
	}
	
	wamb->db = db_open(path_db, flags);
	if(wamb->db == NULL) {
		free(wamb);
		return NULL;
	}

	return wamb;
}



void wamb_close(struct wamb *wamb)
{
	if(wamb->db) {
		db_close(wamb->db);
	}
	free(wamb);
}

int wamb_root_write(struct wamb *wamb, const char *path, dev_t dev, ino_t ino)
{
	char key[PATH_MAX];
	char val[32];
	snprintf(key, sizeof key, "%s", path);
	snprintf(val, sizeof val, "%jd %jd", dev, ino);
	db_put(wamb->db, key, strlen(key), val, strlen(val));
	return 0;
}


struct wamb_node *wamb_node_new(dev_t dev, ino_t ino)
{
	struct wamb_node *node;

	node = malloc(sizeof(struct wamb_node));
	assert(node);
	node->child_count = 0;
	node->child_max = 32;
	node->dev = dev;
	node->ino = ino;

	node->child_list = malloc(sizeof(struct wamb_child) * node->child_max);
	assert(node->child_list);

	return node;
}


void wamb_node_add_child(struct wamb_node *node, const char *name, off_t size, dev_t dev, ino_t ino)
{
	if(node->child_count >= node->child_max) {
		node->child_max *= 2;
		node->child_list = realloc(node->child_list, sizeof(struct wamb_child) * node->child_max);
		assert(node->child_list);
	}

	struct wamb_child *child = &node->child_list[node->child_count];
	node->child_count ++;

	strncpy(child->name, name, sizeof(child->name));
	child->size = size;
	child->dev = dev;
	child->ino = ino;
}


struct wamb_node *wamb_read_node(struct wamb *wamb, dev_t dev, ino_t ino)
{
	char key[32];
	size_t keyl;
	char *val;
	size_t vall;

	keyl = snprintf(key, sizeof key, "d %jd %jd", dev, ino);
	val = db_get(wamb->db, key, keyl, &vall);
	if(val == NULL) return NULL;

	struct wamb_node *node = wamb_node_new(dev, ino);
	node->child_list = (void *)val;
	node->child_count = vall / sizeof(struct wamb_child);

	return node;
}


int wamb_node_write(struct wamb *wamb, struct wamb_node *node)
{
	char key[32];
	snprintf(key, sizeof key, "d %jd %jd", node->dev, node->ino);
	db_put(wamb->db, key, strlen(key), (void *)node->child_list, node->child_count * sizeof(struct wamb_child));
	return 0;
}


struct wamb_node *wamb_find_child(struct wamb *wamb, struct wamb_node *node, const char *name)
{
	size_t i;

	for(i=0; i<node->child_count; i++) {
		struct wamb_child *child = &node->child_list[i];
		if(strcmp(child->name, name) == 0) {
			return wamb_read_node(wamb, child->dev, child->ino);
		}
	}
	return NULL;
}


struct wamb_node *wamb_find_dir(struct wamb *wamb, const char *path)
{
	int l = strlen(path);
	char *val;
	size_t vallen;
	dev_t dev;
	ino_t ino;
	
	/* Find top path in database */

	while(l > 0) {
		val = db_get(wamb->db, path, l, &vallen);
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

	struct wamb_node *node = wamb_read_node(wamb, dev, ino);
	
	char buf[PATH_MAX+1];
	strncpy(buf, path+l, sizeof buf);

	char *save;
	char *name = strtok_r(buf, "/", &save);

	while(node && name) {
		struct wamb_node *node_next = wamb_find_child(wamb, node, name);
		wamb_node_free(node);
		node = node_next;
		name = strtok_r(NULL, "/", &save);
	}

	return node;

}


void wamb_node_free(struct wamb_node *node)
{
	free(node);
}



/*
 * End
 */

