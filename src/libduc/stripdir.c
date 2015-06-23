
/*
 * idea stolen from https://raw.githubusercontent.com/duanev/path-canon-c/master/pathcanon.c
 */

#include "config.h"

#include <string.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

#include "private.h"
#include "utstring.h"


#ifdef TEST

/*
   gcc -Wall -Werror -I. -DTEST -g src/libduc/stripdir.c  && valgrind --quiet --leak-check=full ./a.out 
   x86_64-w64-mingw32-gcc -Wall -Werror -I. -DTEST -g src/libduc/stripdir.c && wine64 ./a.exe 
*/

#define duc_malloc malloc
#define duc_malloc0(n) calloc(n, 1)
#define duc_strdup strdup
#define duc_free free

char *test[][2] = {

	/* absolute */

	{ "/",                       "/" },
	{ "//",                      "/" },
	{ "/home/ico",               "/home/ico" },
	{ "/home/ico/",              "/home/ico" },
	{ "//home/ico/",             "/home/ico" },
	{ "//home///ico/",           "/home/ico" },
	{ "/home/ico/..",            "/home" },
	{ "/home/ico/../ico",        "/home/ico" },
	{ "/home/ico/../..",         "/" },
	{ "/home/./ico",             "/home/ico" },
	{ "/../..",                  "/" },
	
	{ "C:\\Windows\\System32",   "C:/Windows/System32" },
	{ "c:/",                     "C:/" },
	{ "c:\\..\\..\\..",          "C:/" },
	{ "c:\\",                    "C:/" },
	{ "c:\\users\\ico",          "C:/users/ico" },
	{ "c:\\users\\ico\\..",      "C:/users" },

#ifdef WIN32
	{ ".",                       "Z:/home/ico/sandbox/prjs/duc" },
	{ "./..",                    "Z:/home/ico/sandbox/prjs" },
	{ "..",                      "Z:/home/ico/sandbox/prjs" },
	{ "foo/bar",                 "Z:/home/ico/sandbox/prjs/duc/foo/bar" },
	{ "foo/bar/woo/../..",       "Z:/home/ico/sandbox/prjs/duc/foo" },
#else
	{ ".",                       "/home/ico/sandbox/prjs/duc" },
	{ "./..",                    "/home/ico/sandbox/prjs" },
	{ "..",                      "/home/ico/sandbox/prjs" },
	{ "foo/bar",                 "/home/ico/sandbox/prjs/duc/foo/bar" },
	{ "foo/bar/woo/../..",       "/home/ico/sandbox/prjs/duc/foo" },
#endif


	{ "//d/./e/.././o/f/g/./h/../../..//./n/././e/./i/..///", "/d/o/n/e" },

	{ NULL },
};


int main(void)
{
	int i = 0;
	while(test[i][0]) {
		char *o = stripdir(test[i][0]);
		printf("%-40.40s  %-40.40s %s\n", o, test[i][1], strcmp(test[i][1], o) ? "ERR" : "ok");
		duc_free(o);
		i++;
	}

	return 0;
}
 
#endif



static int is_sep(char c)
{
	return c == '/' || c == '\\';
}

struct component {
	const char *name;
	int len;
};


struct splitter {
	struct component *cs;
	int n;
};


static int count(const char *path)
{
	int n = 0;

	while(*path) {
		if(is_sep(*path++)) n++;
	}

	return n;
}


static void split(struct splitter *s, const char *path)
{
	const char *p = path;

	s->cs[s->n].name = path;
	s->cs[s->n].len = 0;

	while(*p) {
		if(is_sep(*p)) {
			s->n++;
			s->cs[s->n].name = p + 1;
			s->cs[s->n].len = 0;
		} else {
			s->cs[s->n].len++;
		}
		p++;
	}
	s->n++;
}


char *stripdir(const char *in)
{
	char cwd[DUC_PATH_MAX] = "";
	char drive = '\0';
	int i, j;

	/* Check if the given path is relative or absolute */

	int in_len = strlen(in);
	int absolute = 0;
	if(in_len >= 1 && is_sep(in[0])) absolute = 1;
	if(in_len >= 3 && isalpha(in[0]) && in[1] == ':' && is_sep(in[2])) {
		drive = in[0];
		absolute = 1;
	}
	
	if(!absolute) {
		if(getcwd(cwd, sizeof(cwd)) == NULL) {
			return NULL;
		}
#ifdef WIN32
		drive = cwd[0];
#endif
	}

	/* Estimate number of parts and alloc space */
	
	int parts = count(in) + 2;
	if(!absolute) parts += count(cwd);

	struct splitter s;
	s.n = 0;
	s.cs = duc_malloc0(parts * sizeof(*s.cs));


	/* Split parts */

	if(!absolute) split(&s, cwd);
	split(&s, in);


	/* canonicalize '.' and '..' */

	for(i=0; i<s.n; i++){
		struct component *c = &s.cs[i];

		/* mark '.' as an empty component */

		if (c->len == 1 && c->name[0] == '.') {
			c->len = 0;
			continue;
		}

		/* '..' eliminates this and the previous non-empty component */
		
		if(c->len == 2 && c->name[0] == '.' && c->name[1] == '.') {
			c->len = 0;
			for(j=i-1; j>=0; j--) {
				if(s.cs[j].len > 0) {
					break;
				}
			}
			if(j>=0) s.cs[j].len = 0;
		}
	}

	UT_string out;
	utstring_init(&out);

	/* Calculate total length of all collected parts */

	if(drive) {
		utstring_printf(&out, "%c:", toupper(drive));
	}

	int n = 0;
	for(i=drive ? 1 : 0; i<s.n; i++){
		struct component *c = &s.cs[i];
		if(c->len > 0) {
			utstring_printf(&out, "/");
			utstring_bincpy(&out, c->name, c->len);
			n++;
		}
	}

	if(n == 0) utstring_printf(&out, "/");

	free(s.cs);
	return utstring_body(&out);	
}


char *stripdir2(const char *dir)
{
	const char * in;
	char * out;
	char * last;
	int ldots;

	int maxlen = DUC_PATH_MAX;
	char *buf = duc_malloc(maxlen);
	in   = dir;
	out  = buf;
	last = buf + maxlen;
	ldots = 0;
	*out  = 0;


	if (!is_sep(*in)) {
		if (getcwd(buf, maxlen - 2) ) {
			out = buf + strlen(buf) - 1;
			if (!is_sep(*out)) *(++out) = '/';
			out++;
		}
		else {
			free(buf);
			return NULL;
		}
	}

	while (out < last) {
		*out = *in;

		if (is_sep(*in))
		{
			while (*(++in) == '/') ;
			in--;
		}

		if (is_sep(*in) || !*in)
		{
			if (ldots == 1 || ldots == 2) {
				while (ldots > 0 && --out > buf)
				{
					if (*out == '/')
						ldots--;
				}
				*(out+1) = 0;
			}
			ldots = 0;

		} else if (*in == '.' && ldots > -1) {
			ldots++;
		} else {
			ldots = -1;
		}

		out++;

		if (!*in)
			break;

		in++;
	}

	if (*in) {
		errno = ENOMEM;
		free(buf);
		return NULL;
	}

	while (--out != buf && (is_sep(*out) || !*out)) *out=0;
	return buf;
}


/*
 * End
 */

