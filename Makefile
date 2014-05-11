BIN   	= ps
SRC 	= index.c dump.c

CFLAGS  += -Wall -Werror
CFLAGS	+= -O3 -g
CFLAGS	+= -D_FILE_OFFSET_BITS=64 
CFLAGS	+= -D_GNU_SOURCE

LDFLAGS += -g

CFLAGS  += $(shell pkg-config --cflags cairo kyotocabinet)
LDFLAGS += $(shell pkg-config --libs cairo kyotocabinet)
LDFLAGS += -lbsd

CROSS	=
OBJS    = $(subst .c,.o, $(SRC))
CC 	= $(CROSS)gcc
LD 	= $(CROSS)gcc

.c.o:
	$(CC) $(CFLAGS) -c $<

$(BIN):	$(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

clean:	
	rm -f $(OBJS) $(BIN) core
