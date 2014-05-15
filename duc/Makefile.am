bin_PROGRAMS = duc
duc_SOURCES = draw.c index.c ls.c main.c

AM_CFLAGS = @TC_CFLAGS@ @CAIRO_CFLAGS@ @PANGO_CFLAGS@ -I../libduc
AM_LDFLAGS= @TC_LIBS@ @CAIRO_LIBS@ @PANGO_LIBS@

# All program(s) are linked with the following.

LDADD = ../libduc/libduc.a
