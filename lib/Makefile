
LIB	:= libwamb.a

SRC	+= wamb.c index.c dir.c varint.c
SRC	+= db-tokyo.c

CFLAGS	+= $(shell pkg-config --cflags tokyocabinet)
LDFLAGS	+= $(shell pkg-config --libs tokyocabinet)

include ../Rules.mak

