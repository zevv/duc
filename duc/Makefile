
BIN	:= duc
SRC	:= main.c index.c ls.c test.c draw.c

CFLAGS	+= -I../libduc
LDFLAGS	+= -L../libduc -lduc -lm

CFLAGS	+= $(shell pkg-config --cflags tokyocabinet cairo)
LDFLAGS	+= $(shell pkg-config --libs tokyocabinet cairo)

include ../Rules.mak

