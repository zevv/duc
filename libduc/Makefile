
LIB	:= libduc.a

SRC	+= duc.c index.c dir.c buffer.c varint.c graph.c
SRC	+= db-tokyo.c

CFLAGS	+= $(shell pkg-config --cflags tokyocabinet cairo pango)
LDFLAGS	+= $(shell pkg-config --libs tokyocabinet cairo pango)

include ../Rules.mak

