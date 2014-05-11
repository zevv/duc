BIN   	= wamb
SRC 	= main.c index.c ls.c db.c

CFLAGS  += -Wall -Werror -m32
CFLAGS	+= -O3 -g
CFLAGS	+= -D_FILE_OFFSET_BITS=64 
CFLAGS	+= -D_GNU_SOURCE

LDFLAGS += -g -m32

CFLAGS  += $(shell pkg-config --cflags tokyocabinet)
LDFLAGS += $(shell pkg-config --libs tokyocabinet)

CROSS	=
OBJS    = $(subst .c,.o, $(SRC))
CC 	= $(CROSS)gcc
LD 	= $(CROSS)gcc

.c.o:
	$(CC) $(CFLAGS) -c $<

$(BIN):	$(OBJS)
	$(LD) -o $@ $(OBJS) $(LDFLAGS)

clean:	
	rm -f $(OBJS) $(BIN) core
