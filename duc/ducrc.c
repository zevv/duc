
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>

#include "private.h"
#include "duc.h"
#include "ducrc.h"


struct item {
	char *section;
	char *key;
	char *val;
	struct item *next;
};

struct ducrc {
	struct item *item_list;
};


struct ducrc *ducrc_new(void)
{
	struct ducrc *ducrc;

	ducrc = duc_malloc(sizeof ducrc);
	ducrc->item_list = NULL;

	return ducrc;
}


void ducrc_free(struct ducrc *ducrc)
{
	struct item *i, *in;

	i = ducrc->item_list;
	while(i) {
		in = i->next;
		duc_free(i->section);
		duc_free(i->key);
		duc_free(i->val);
		duc_free(i);
		i = in;
	}
	duc_free(ducrc);
}


void ducrc_dump(struct ducrc *ducrc)
{
	struct item *i = ducrc->item_list;

	while(i) {
		duc_log(NULL, DUC_LOG_DMP, "%s.%s = %s\n", i->section, i->key, i->val);
		i = i->next;
	}
}


/* 
 * Trim whitespace off string. Modifies the string, and returns pointer
 * to first non-whitespace char. 
 */

static char *trim(char *s)
{
	char *e;
	while(isspace(*s)) s++;
	if(*s == 0) return s;
	e = s + strlen(s) - 1;
	while(e > s && isspace(*e)) e--;
	*(e+1) = 0;
	return s;
}


int ducrc_read(struct ducrc *ducrc, const char *path)
{

	FILE *f = fopen(path, "r");
	if(f == NULL) {
		duc_log(NULL, DUC_LOG_DBG, "Not reading ducrciguration from '%s': %s", path, strerror(errno));
		return -1;
	}

	char section[256] = "";
	char buf[256];

	while(fgets(buf, sizeof buf, f) != NULL) {

		char *p;

		p = strchr(buf, '\n'); if(p) *p = '\0';
		p = strchr(buf, '\r'); if(p) *p = '\0';

		/* section? */

		if(buf[0] == '[') {
			p = strchr(buf, ']');
			if(p) {
				*p = 0;
				strncpy(section, buf+1, sizeof(section));
			}
		}

		/* key + value ? */

		p = strchr(buf, '=');

		if(p) {
			*p = '\0';
			char *key = trim(buf);
			char *val = trim(p + 1);

			struct item *i;
			i = duc_malloc(sizeof *i);
			i->section = strdup(section);
			i->key = strdup(key);
			i->val = strdup(val);

			i->next = ducrc->item_list;
			ducrc->item_list = i;
		}
	}
		

	fclose(f);

	return 0;
}


static struct item *item_find(struct ducrc *ducrc, const char *section, const char *key)
{
	struct item *item = ducrc->item_list;

	while(item) {
		if(strcmp(section, item->section) == 0 && strcmp(key, item->key) == 0) {
			return item;
		}
		item = item->next;
	}
	return NULL;
}


/*
 * Set string, overwrite existing item if already exists
 */

void ducrc_set_str(struct ducrc *ducrc, const char *section, const char *key, const char *val)
{
	struct item *item;

	item = item_find(ducrc, section, key);

	if(item == NULL) {
		item = duc_malloc(sizeof *item);
		item->key = duc_strdup(key);
		item->section = duc_strdup(section);
	} else {
		duc_free(item->val);
	}

	item->val = duc_strdup(val);

	item->next = ducrc->item_list;
	ducrc->item_list = item;
}


void ducrc_set_int(struct ducrc *ducrc, const char *section, const char *key, int val)
{
	char s[32];
	snprintf(s, sizeof(s), "%d", val);
	ducrc_set_str(ducrc, section, key, s);
}


const char *ducrc_get_str(struct ducrc *ducrc, const char *section, const char *key)
{
	struct item *item = item_find(ducrc, section, key);
	assert(item);
	return item->val;
}


int ducrc_get_int(struct ducrc *ducrc, const char *section, const char *key)
{
	const char *val = ducrc_get_str(ducrc, section, key);
	return atoi(val);
}


/*
 * End
 */
