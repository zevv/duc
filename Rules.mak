

CFLAGS  += -Wall -Werror
CFLAGS	+= -O3 -g
CFLAGS	+= -D_FILE_OFFSET_BITS=64 
CFLAGS	+= -D_GNU_SOURCE
CFLAGS	+= -fPIC

LDFLAGS += -g

CROSS	=
OBJS    = $(subst .c,.o, $(SRC))
CC 	= $(CROSS)gcc
LD 	= $(CROSS)gcc
AR	= $(CROSS)ar

ifdef verbose
P       := @true
E       :=
MAKE    := make
else
P       := @echo
E       := @
MAKE    := make -s
endif

all: $(SUBDIRS) $(BIN) $(LIB) $(SHLIB)

.PHONY: $(SUBDIRS)
$(SUBDIRS):
	$(P) " [SUB  ] $@"
	$(E) $(MAKE) -C $@

.c.o:
	$(P) " [CC   ] $@"
	$(E) $(CC) $(CFLAGS) -c $<

$(BIN):	$(OBJS)
	$(P) " [LD   ] $@"
	$(E) $(LD) -o $@ $(OBJS) $(LDFLAGS)

$(SHLIB): $(OBJS)
	$(P) " [LD   ] $@"
	$(E) $(LD) -fPIC -shared -o $@ $(OBJS) $(LDFLAGS)

$(LIB): $(OBJS)
	$(P) " [LD   ] $@"
	$(E) $(AR) $(ARFLAGS) $@ $(OBJS)

clean:	
	$(P) " [CLEAN] $@"
	$(E) rm -f $(OBJS) $(BIN) $(LIB) $(SHLIB) core perf.data*
	$(E) for dir in $(SUBDIRS); do $(MAKE) -C $$dir clean; done

