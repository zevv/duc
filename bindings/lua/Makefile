
LUAVER  := 5.2

CFLAGS  += -g
CFLAGS  += -Wall
CFLAGS  += -shared -fPIC
CFLAGS  += $(shell pkg-config --cflags --libs lua-$(LUAVER))

LDFLAGS	+= -lduc

lduc.so: lduc.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@

clean:
	rm -f lduc.so
