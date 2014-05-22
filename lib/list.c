
#include <stdlib.h>
#include <assert.h>

#include "list.h"


void list_push(struct list **list, void *data)
{
	struct list *ln = malloc(sizeof(struct list));
	assert(ln);
	ln->data = data;
	ln->next = *list;
	*list = ln;
}


void *list_pop(struct list **list)
{
	struct list *tmp = *list;
	if(tmp) {
		void *data = tmp->data;
		*list = tmp->next;
		free(tmp);
		return data;
	}
	return NULL;
}


void list_free(struct list *list, void(*fn)(void *data))
{
	while(list) {
		if(fn) fn(list->data);
		struct list *tmp = list;
		free(list);
		list = tmp;
	}
}


/*
 * End
 */
