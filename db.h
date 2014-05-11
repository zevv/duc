#ifndef db_h
#define db_h

struct db;
struct node;

struct db *db_open(void);
void db_close(struct db *db);

struct node *node_find(struct db *db, const char *path)


#endif
