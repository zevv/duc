bin_PROGRAMS = duc
duc_SOURCES = draw.c index.c ls.c main.c

AM_CFLAGS = @TC_CFLAGS@ @PANGO_CFLAGS@ @CAIRO_CFLAGS@ -I../libduc
AM_LDFLAGS= @TC_LIBS@ @PANGO_LIBS@ @CAIRO_LIBS@

LDADD = -L../libduc -lduc

# All program(s) are linked with the following.

