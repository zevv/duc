CFLAGS = -Wall -Werror
AM_CFLAGS= @TC_CFLAGS@ @CAIRO_CFLAGS@ @PANGO_CFLAGS@

lib_LTLIBRARIES = libduc.la
libduc_la_SOURCES=duc.c index.c dir.c buffer.c varint.c graph.c db-tokyo.c list.c
libduc_la_LDFLAGS = -version-info @LIB_CURRENT@:@LIB_REVISION@:@LIB_AGE@


