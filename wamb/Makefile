
BIN	:= wamb
SRC	:= main.c index.c ls.c test.c

CFLAGS	+= -I../libwamb
LDFLAGS	+= -L../libwamb -lwamb

CFLAGS	+= $(shell pkg-config --cflags tokyocabinet)
LDFLAGS	+= $(shell pkg-config --libs tokyocabinet)

include ../Rules.mak

