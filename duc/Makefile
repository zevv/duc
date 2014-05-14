
BIN	:= duc
SRC	:= main.c index.c ls.c draw.c cgi.c info.c

CFLAGS	+= -I../libduc
LDFLAGS	+= -L../libduc -lduc -lm

CFLAGS	+= $(shell pkg-config --cflags tokyocabinet cairo pango pangocairo)
LDFLAGS	+= $(shell pkg-config --libs tokyocabinet cairo pango pangocairo) 

include ../Rules.mak

