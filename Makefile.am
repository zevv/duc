
bin_PROGRAMS := duc

duc_SOURCES := \
	src/libduc/buffer.c \
	src/libduc/buffer.h \
	src/libduc/db.c \
	src/libduc/db.h \
	src/libduc/db-tokyo.c \
	src/libduc/db-kyoto.c \
	src/libduc/db-leveldb.c \
	src/libduc/db-sqlite3.c \
	src/libduc/db-lmdb.c \
	src/libduc/dir.c \
	src/libduc/duc.c \
	src/libduc/duc.h \
	src/libduc/index.c \
	src/libduc/private.h \
	src/libduc/canonicalize.c \
	src/libduc/varint.c \
	src/libduc/varint.h \
	src/libduc/uthash.h \
	src/libduc/utlist.h \
	src/libduc/utstring.h

duc_SOURCES += \
	src/glad/glad.c \
	src/glad/KHR/khrplatform.h \
	src/glad/glad/glad.h

duc_SOURCES += \
	src/libduc-graph/graph.c \
	src/libduc-graph/graph-cairo.c \
	src/libduc-graph/graph-opengl.c \
	src/libduc-graph/graph-svg.c \
	src/libduc-graph/graph-html.c \
	src/libduc-graph/graph-private.h \
	src/libduc-graph/duc-graph.h

duc_SOURCES  += \
	src/duc/cmd-cgi.c \
	src/duc/cmd-graph.c \
	src/duc/cmd-gui.c \
	src/duc/cmd-guigl.c \
	src/duc/cmd.h \
	src/duc/cmd.h  \
	src/duc/cmd-index.c \
	src/duc/cmd-info.c \
	src/duc/cmd-ls.c \
	src/duc/cmd-ui.c \
	src/duc/cmd-xml.c \
	src/duc/cmd-json.c \
	src/duc/ducrc.c \
	src/duc/ducrc.h \
	src/duc/main.c


AM_CFLAGS := @CAIRO_CFLAGS@ @PANGO_CFLAGS@ @PANGOCAIRO_CFLAGS@
AM_CFLAGS += @TC_CFLAGS@ @SQLITE3_CFLAGS@ @GLFW3_CFLAGS@ @LMDB_CFLAGS@ @KC_CFLAGS@
AM_CFLAGS += -Isrc/libduc -Isrc/libduc-graph -Isrc/glad

duc_LDADD := @CAIRO_LIBS@ @PANGO_LIBS@ @PANGOCAIRO_LIBS@
duc_LDADD += @TC_LIBS@ @SQLITE3_LIBS@ @GLFW3_LIBS@ @LMDB_LIBS@ @KC_LIBS@

man1_MANS = \
	doc/duc.1

EXTRA_DIST = \
	doc/duc.1 \
	doc/duc.1.html \
	src/libduc-graph/font.c \
	doc/duc.md

install-exec-hook:
	-/sbin/ldconfig || :;

