
BIN	:= duc
SRC	:= main.c index.c ls.c test.c

CFLAGS	+= -I../libduc
LDFLAGS	+= -L../libduc -lduc

CFLAGS	+= $(shell pkg-config --cflags tokyocabinet)
LDFLAGS	+= $(shell pkg-config --libs tokyocabinet)

include ../Rules.mak

