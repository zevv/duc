AM_CFLAGS := @TC_CFLAGS@ @CAIRO_CFLAGS@ @PANGO_CFLAGS@ @PANGOCAIRO_CFLAGS@ -I../lib -Wall -Werror

bin_PROGRAMS := duc duc.debug
duc_SOURCES  := graph.c index.c ls.c main.c info.c cgi.c xml.c gui.c ducrc.c
duc_SOURCES  += cmd.h ducrc.h
duc_LDADD    := ../lib/libduc.la ../lib/libduc-graph.la @TC_LIBS@ @CAIRO_LIBS@ @PANGO_LIBS@ @PANGOCAIRO_LIBS@

# Create an easier-to-debug version of duc.
duc_debug_SOURCES = graph.c index.c ls.c main.c info.c cgi.c xml.c gui.c ducrc.c
duc_debug_LDADD = ../lib/libduc.la ../lib/libduc-graph.la @TC_LIBS@ @CAIRO_LIBS@ @PANGO_LIBS@ @PANGOCAIRO_LIBS@
duc_debug_LDFLAGS = -static

