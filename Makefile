BIN   	= ps
SRC 	= main.c index.c dump.c db.c

CFLAGS  += -Wall -Werror
CFLAGS	+= -O3 -g
CFLAGS	+= -D_FILE_OFFSET_BITS=64 
CFLAGS	+= -D_GNU_SOURCE

LDFLAGS += -g

CFLAGS  += $(shell pkg-config --cflags cairo tokyocabinet)
LDFLAGS += $(shell pkg-config --libs cairo tokyocabinet)

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
