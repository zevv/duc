CC      = gcc
CFLAGS  = -O2 -Wall -Wextra -std=c11
LDFLAGS =

# ---------------------------------------------------------------------------
# Auto-detect available backends via pkg-config (or direct lib checks).
# Each enabled backend appends -DHAVE_<NAME> plus its compile/link flags.
# ---------------------------------------------------------------------------

# --- Tokyo Cabinet ----------------------------------------------------------
TC_LIBS := $(shell pkg-config --libs   tokyocabinet 2>/dev/null)
TC_CFLAGS := $(shell pkg-config --cflags tokyocabinet 2>/dev/null)
ifneq ($(TC_LIBS),)
  CFLAGS  += -DHAVE_TOKYOCABINET $(TC_CFLAGS)
  LDFLAGS += $(TC_LIBS)
  $(info [+] Tokyo Cabinet detected)
else
  $(info [-] Tokyo Cabinet not found  (install: libtokyocabinet-dev))
endif

# --- Kyoto Cabinet ----------------------------------------------------------
KC_LIBS := $(shell pkg-config --libs   kyotocabinet 2>/dev/null)
KC_CFLAGS := $(shell pkg-config --cflags kyotocabinet 2>/dev/null)
ifneq ($(KC_LIBS),)
  CFLAGS  += -DHAVE_KYOTOCABINET $(KC_CFLAGS)
  LDFLAGS += $(KC_LIBS)
  $(info [+] Kyoto Cabinet detected)
else
  $(info [-] Kyoto Cabinet not found  (install: libkyotocabinet-dev))
endif

# --- LevelDB ----------------------------------------------------------------
LDB_LIBS := $(shell pkg-config --libs   leveldb 2>/dev/null)
LDB_CFLAGS := $(shell pkg-config --cflags leveldb 2>/dev/null)
ifneq ($(LDB_LIBS),)
  CFLAGS  += -DHAVE_LEVELDB $(LDB_CFLAGS)
  LDFLAGS += $(LDB_LIBS)
  $(info [+] LevelDB detected)
else
  # Fallback: try direct link (common on Debian/Ubuntu without .pc file)
  LDB_DIRECT := $(shell echo 'int main(){}' | $(CC) -x c - -lleveldb -o /dev/null 2>/dev/null && echo yes)
  ifeq ($(LDB_DIRECT),yes)
    CFLAGS  += -DHAVE_LEVELDB
    LDFLAGS += -lleveldb
    $(info [+] LevelDB detected (direct link))
  else
    $(info [-] LevelDB not found       (install: libleveldb-dev))
  endif
endif

# --- SQLite3 ----------------------------------------------------------------
SQ_LIBS := $(shell pkg-config --libs   sqlite3 2>/dev/null)
SQ_CFLAGS := $(shell pkg-config --cflags sqlite3 2>/dev/null)
ifneq ($(SQ_LIBS),)
  CFLAGS  += -DHAVE_SQLITE3 $(SQ_CFLAGS)
  LDFLAGS += $(SQ_LIBS)
  $(info [+] SQLite3 detected)
else
  $(info [-] SQLite3 not found         (install: libsqlite3-dev))
endif

# --- LMDB -------------------------------------------------------------------
MDB_DIRECT := $(shell echo 'int main(){}' | $(CC) -x c - -llmdb -o /dev/null 2>/dev/null && echo yes)
ifeq ($(MDB_DIRECT),yes)
  CFLAGS  += -DHAVE_LMDB
  LDFLAGS += -llmdb
  $(info [+] LMDB detected)
else
  $(info [-] LMDB not found            (install: liblmdb-dev))
endif

# --- Tkrzw ------------------------------------------------------------------
TKRZW_DIRECT := $(shell echo 'int main(){}' | $(CC) -x c - -ltkrzw -o /dev/null 2>/dev/null && echo yes)
ifeq ($(TKRZW_DIRECT),yes)
  CFLAGS  += -DHAVE_TKRZW
  LDFLAGS += -ltkrzw
  $(info [+] Tkrzw detected)
else
  $(info [-] Tkrzw not found           (install: libtkrzw-dev))
endif

# ---------------------------------------------------------------------------

TARGET = migrator

.PHONY: all clean

all: $(TARGET)

$(TARGET): migrator.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET)
