
BIN	:= wamb
SRC	:= main.c index.c ls.c

CFLAGS	+= -I../lib
LDFLAGS	+= -L../lib -lwamb

CFLAGS	+= $(shell pkg-config --cflags tokyocabinet)
LDFLAGS	+= $(shell pkg-config --libs tokyocabinet)

include ../Rules.mak

