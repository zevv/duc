bin_PROGRAMS = duc
duc_SOURCES = draw.c index.c ls.c main.c info.c cgi.c

AM_CFLAGS = @TC_CFLAGS@ @CAIRO_CFLAGS@ @PANGO_CFLAGS@ @PANGOCAIRO_CFLAGS@ -I../libduc
LDADD = ../libduc/libduc.a @TC_LIBS@ @CAIRO_LIBS@ @PANGO_LIBS@ @PANGOCAIRO_LIBS@
