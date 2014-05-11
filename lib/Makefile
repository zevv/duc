
LIB	:= libwamb.a
SRC	:= db.c index.c

CFLAGS	+= $(shell pkg-config --cflags tokyocabinet)
LDFLAGS	+= $(shell pkg-config --libs tokyocabinet)

include ../Rules.mak

