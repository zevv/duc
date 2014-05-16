
#ifndef list_h
#define list_h

struct list {
	char *data;
	struct list *next;
};

void list_push(struct list **list, char *data);
void *list_pop(struct list **list);
void list_free(struct list *list, void(*fn)(void *data));

#endif
