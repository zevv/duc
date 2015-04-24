AM_CFLAGS := @TC_CFLAGS@ @CAIRO_CFLAGS@ @PANGO_CFLAGS@ @PANGOCAIRO_CFLAGS@ -I../lib -Wall

bin_PROGRAMS := duc
duc_SOURCES  := cmd-graph.c cmd-index.c cmd-ls.c main.c cmd-info.c cmd-cgi.c cmd-xml.c cmd-gui.c ducrc.c cmd-ui.c
duc_SOURCES  += cmd.h ducrc.h
duc_LDADD    := ../lib/libduc.la ../lib/libduc-graph.la @TC_LIBS@ @CAIRO_LIBS@ @PANGO_LIBS@ @PANGOCAIRO_LIBS@

