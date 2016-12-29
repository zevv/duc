
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include <getopt.h>

#include "private.h"
#include "duc.h"
#include "ducrc.h"

#define MAX_OPTIONS 64


struct ducrc {
	char *section;
	int noptions;
	struct ducrc_option *option_list[MAX_OPTIONS];
};


struct ducrc *ducrc_new(const char *section)
{
	struct ducrc *ducrc;

	ducrc = duc_malloc(sizeof *ducrc);
	ducrc->noptions = 0;
	ducrc->section = duc_strdup(section);

	return ducrc;
}


void ducrc_free(struct ducrc *ducrc)
{
	duc_free(ducrc->section);
	duc_free(ducrc);
}


void ducrc_add_options(struct ducrc *ducrc, struct ducrc_option *o)
{
	while(o && o->longopt) {
		if(ducrc->noptions < MAX_OPTIONS) {
			ducrc->option_list[ducrc->noptions++] = o;
		}
		o++;
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


static void handle_opt(struct ducrc *ducrc, char shortopt, const char *longopt, const char *val)
{
	struct ducrc_option **os = ducrc->option_list;
	struct ducrc_option *o = NULL;
	bool *v_bool;
	int *v_int;
	double *v_double;
	const char **v_string;
	int i;
	void (*fn)(const char *val);

	/* Find option */

	for(i=0; i<ducrc->noptions; i++) {
		o = *os;
		if(shortopt && shortopt == o->shortopt) goto found;
		if(longopt && strcmp(longopt, o->longopt) == 0) goto found;
		os++;
	}

	if(shortopt) {
		fprintf(stderr, "Unknown option '%c'\n", shortopt);
	} else {
		fprintf(stderr, "Unknown option '%s'\n", longopt);
	}

	return;

found:

	/* Handle option */

	switch(o->type) {

		case DUCRC_TYPE_BOOL:
			v_bool = o->ptr;
			*v_bool = 1;
			break;

		case DUCRC_TYPE_INT:
			v_int = o->ptr;
			*v_int = atoi(val);
			break;

		case DUCRC_TYPE_DOUBLE:
			v_double = o->ptr;
			*v_double = atof(val);
			break;

		case DUCRC_TYPE_STRING:
			v_string = o->ptr;
			*v_string = strdup(val);
			break;

		case DUCRC_TYPE_FUNC:
			fn = o->ptr;
			fn(val);
			break;
	}

}


int ducrc_read(struct ducrc *ducrc, const char *path)
{

	FILE *f = fopen(path, "r");
	if(f == NULL) {
		//duc_log(NULL, DUC_LOG_DBG, "Not reading configuration from '%s': %s", path, strerror(errno));
		return -1;
	}

	char section[256] = "";
	char buf[256];

	while(fgets(buf, sizeof buf, f) != NULL) {

		char *l = trim(buf);
		char *p;

		/* Strip newlines and comments */

		p = strchr(l, '#'); if(p) *p = '\0';
		p = strchr(l, '\n'); if(p) *p = '\0';
		p = strchr(l, '\r'); if(p) *p = '\0';

		/* section? */

		if(l[0] == '[') {
			p = strchr(l, ']');
			if(p) {
				*p = 0;
				strncpy(section, l+1, sizeof(section));
			}
			continue;
		}

		if(strlen(section) == 0 || 
				strcmp(section, "global") == 0 || 
				strcmp(section, ducrc->section) == 0) {

			/* key + value ? */

			p = strchr(l, ' ');

			if(p) {
				*p = '\0';
				char *longopt = trim(l);
				char *val = trim(p + 1);
				handle_opt(ducrc, 0, longopt,  val);
				continue;
			}

			/* longopt only */

			char *longopt = trim(l);
			if(strlen(longopt) > 0) {
				handle_opt(ducrc, 0, longopt, NULL);
			}
		}
	}
		

	fclose(f);

	return 0;
}


/*
 * I tried to implement a standard-style getopt_long function for parsing the
 * command line into duc options. This is hard work, and it has been a long
 * day. Instead of parsing myself, this function creates a 'struct option' list
 * from the duc options and relies on POSIX getopt_long() to do the hard work.
 */

int ducrc_getopt(struct ducrc *ducrc, int *argc, char **argv[])
{
	struct option longopts[MAX_OPTIONS + 1] = { { NULL } };
	char optstr[MAX_OPTIONS*2] = "";
	int n = 0;
	int l = 0;

	/* Prepare a list of options for getopt_long() */

	struct ducrc_option **os = ducrc->option_list;

	int i;
	for(i=0; i<ducrc->noptions; i++) {
		struct ducrc_option *o = *os;
		
		longopts[n].name = o->longopt;
		if(o->shortopt) {
			l += sprintf(optstr+l, "%c", o->shortopt);
			longopts[n].val = o->shortopt;
		}

		if(o->type == DUCRC_TYPE_BOOL) {
			longopts[n].has_arg = no_argument;
		} else {
			longopts[n].has_arg = required_argument;
			if(o->shortopt) l += sprintf(optstr+l, ":");
		}

		n++;
		os++;
	}

	/* Rely on libc to do the hard work */

	int c;
	int idx;

	if(*argc > 1) optind = 2;

	while( ( c = getopt_long(*argc, *argv, optstr, longopts, &idx)) != -1) {
		if(c == '?') return -1;
		handle_opt(ducrc, c, c ? 0 : longopts[idx].name, optarg);
	}
	
	*argc -= optind;
	*argv += optind;

	return 0;
}


/*
 * End
 */
