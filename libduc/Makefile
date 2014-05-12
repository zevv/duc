
LIB	:= libduc.a

SRC	+= duc.c index.c dir.c buffer.c varint.c
SRC	+= db-tokyo.c

CFLAGS	+= $(shell pkg-config --cflags tokyocabinet)
LDFLAGS	+= $(shell pkg-config --libs tokyocabinet)

include ../Rules.mak

