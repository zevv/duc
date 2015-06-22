
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

#ifndef WIN32

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


#else

/*
realpath() Win32 implementation
By Nach M. S.
Copyright (C) September 8, 2005

I am placing this in the public domain for anyone to use or modify
*/

#include <windows.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>

char *stripdir(const char *path)
{
	char *return_path = 0;

	if (path)
	{
		return_path = malloc(PATH_MAX);

		if (return_path)
		{
			size_t size = GetFullPathNameA(path, PATH_MAX, return_path, 0);

			if (size > PATH_MAX)
			{
				size_t new_size;

				free(return_path);
				return_path = malloc(size);

				if (return_path)
				{
					new_size = GetFullPathNameA(path, size, return_path, 0);

					if (new_size > size)
					{
						free(return_path);
						return_path = 0;
						errno = ENAMETOOLONG;
					}
					else
					{
						size = new_size;
					}
				}
				else
				{
					errno = EINVAL;
				}
			}

			if (!size)
			{
				free(return_path);

				return_path = 0;

				switch (GetLastError())
				{
					case ERROR_FILE_NOT_FOUND:
						errno = ENOENT;
						break;

					case ERROR_PATH_NOT_FOUND: case ERROR_INVALID_DRIVE:
						errno = ENOTDIR;
						break;

					case ERROR_ACCESS_DENIED:
						errno = EACCES;
						break;

					default:
						errno = EIO;
						break;
				}
			}

			if (return_path)
			{
				struct stat stat_buffer;


				if (stat(return_path, &stat_buffer))
				{
					free(return_path);

					return_path = 0;

				}
			}
		}
		else
		{
			errno = EINVAL;
		}
	}
	else
	{
		errno = EINVAL;
	}

	return return_path;
}

#endif

