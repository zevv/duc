
LIB	:= libwamb.a

SRC	+= wamb.c index.c list.c
SRC	+= db_raw_tokyo.c

CFLAGS	+= $(shell pkg-config --cflags tokyocabinet)
LDFLAGS	+= $(shell pkg-config --libs tokyocabinet)

include ../Rules.mak

