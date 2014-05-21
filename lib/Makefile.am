CFLAGS = -Wall -Werror

lib_LTLIBRARIES = libduc.la libduc-graph.la

libduc_la_SOURCES = duc.c index.c dir.c buffer.c varint.c db-tokyo.c list.c
libduc_la_LDFLAGS = -version-info @LIB_CURRENT@:@LIB_REVISION@:@LIB_AGE@
libduc_la_CFLAGS= @TC_CFLAGS@ 

libduc_graph_la_SOURCES = graph.c
libduc_graph_la_LDFLAGS = -version-info @LIB_CURRENT@:@LIB_REVISION@:@LIB_AGE@
libduc_graph_la_CFLAGS= @CAIRO_CFLAGS@ @PANGO_CFLAGS@

