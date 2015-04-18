AM_CFLAGS := @TC_CFLAGS@ @CAIRO_CFLAGS@ @PANGO_CFLAGS@ @PANGOCAIRO_CFLAGS@ -I../lib -Wall

bin_PROGRAMS := duc duc.debug
duc_SOURCES  := cmd-graph.c cmd-index.c cmd-ls.c main.c cmd-info.c cmd-cgi.c cmd-xml.c cmd-gui.c ducrc.c
duc_SOURCES  += cmd.h ducrc.h
duc_LDADD    := ../lib/libduc.la ../lib/libduc-graph.la @TC_LIBS@ @CAIRO_LIBS@ @PANGO_LIBS@ @PANGOCAIRO_LIBS@

# Create an easier-to-debug version of duc.
duc_debug_SOURCES = $(duc_SOURCES)
duc_debug_LDADD = ../lib/libduc.la ../lib/libduc-graph.la @TC_LIBS@ @CAIRO_LIBS@ @PANGO_LIBS@ @PANGOCAIRO_LIBS@
duc_debug_LDFLAGS = -static

