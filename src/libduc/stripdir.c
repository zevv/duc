
/*
 * stripdir(), mostly stolen from the realpath command line utility
 *
 * Copyright: (C) 1996 Lars Wirzenius <liw@iki.fi>
 *            (C) 1996-1998 Jim Pick <jim@jimpick.com>
 *            (C) 2001-2009 Robert Luberda <robert@debian.org>
 */

#include "config.h"

#include <string.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include "private.h"

char *stripdir(const char *dir)
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


	if (*in != '/') {
		if (getcwd(buf, maxlen - 2) ) {
			out = buf + strlen(buf) - 1;
			if (*out != '/') *(++out) = '/';
			out++;
		}
		else {
			free(buf);
			return NULL;
		}
	}

	while (out < last) {
		*out = *in;

		if (*in == '/')
		{
			while (*(++in) == '/') ;
			in--;
		}

		if (*in == '/' || !*in)
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

	while (--out != buf && (*out == '/' || !*out)) *out=0;
	return buf;
}

