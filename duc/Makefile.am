
CFLAGS = -Wall -Werror

bin_PROGRAMS = duc
duc_SOURCES = graph.c index.c ls.c main.c info.c cgi.c xml.c gui.c

AM_CFLAGS = @TC_CFLAGS@ @CAIRO_CFLAGS@ @PANGO_CFLAGS@ @PANGOCAIRO_CFLAGS@ -I../lib
LDADD = ../lib/libduc.a @TC_LIBS@ @CAIRO_LIBS@ @PANGO_LIBS@ @PANGOCAIRO_LIBS@
