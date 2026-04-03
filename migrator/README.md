# duc Database Migrator

A standalone command-line tool that converts a duc index database from any
supported backend format to any other, without losing data.

For a detailed description of each backend's on-disk format, internal
structure, and quirks see **[db-formats.md](db-formats.md)**.

---

## Overview

duc stores its index as a simple key-value database.  The backend is chosen
at compile time; all backends share the same logical schema but differ in
file format, compression, and performance characteristics.

`migrator` links every available backend into a single binary and performs a
raw KV copy between them — all duc-internal keys (`duc_db_version`,
`duc_index_reports`, path records, …) are transferred verbatim.

Typical use cases:

- Upgrading from the 1.4.6 default (`tokyocabinet`) to the 1.5.0 default (`tkrzw`)
- Converting a `leveldb` directory-based database to a single-file format
- Switching to `sqlite3` for inspection with standard SQL tooling

---

## Supported Backends

| Backend         | Format  | Compression       | Default in  |
|-----------------|---------|-------------------|-------------|
| `tokyocabinet`  | File    | Optional (deflate)| 1.4.6       |
| `kyotocabinet`  | File    | Always (kct)      | —           |
| `leveldb`       | **Dir** | Always (Snappy)   | —           |
| `sqlite3`       | File    | None              | —           |
| `lmdb`          | File    | None              | —           |
| `tkrzw`         | File    | Optional (ZSTD)   | 1.5.0-rc2   |

All backends listed above are compiled into one binary if the corresponding
library is present at build time.  The Makefile reports which ones were
detected.

> **Note on LevelDB:** the `path` for a LevelDB database is a **directory**,
> not a file.  Pass the directory path to `--from` or `--to` accordingly.

---

## Building

Dependencies are auto-detected via `pkg-config` (and direct linker probes for
LMDB and Tkrzw, which often lack `.pc` files).

```sh
cd migrator
make
```

Example output showing which backends were found:

```
[+] Tokyo Cabinet detected
[-] Kyoto Cabinet not found  (install: libkyotocabinet-dev)
[+] LevelDB detected (direct link)
[+] SQLite3 detected
[+] LMDB detected
[+] Tkrzw detected
```

At least two backends must be compiled in to perform a migration.

### Manual flags

If auto-detection fails you can pass flags directly:

```sh
make CFLAGS="-DHAVE_TOKYOCABINET -DHAVE_TKRZW" \
     LDFLAGS="-ltokyocabinet -ltkrzw"
```

---

## Usage

```
./migrator --from <format>:<path> --to <format>:<path>
```

`format` is one of the backend names in the table above; `path` is the
filesystem path to the database file (or directory for LevelDB).

### Examples

**Tokyo Cabinet → Tkrzw** (the common 1.4.6 → 1.5.0 upgrade path):

```sh
./migrator \
  --from tokyocabinet:~/.cache/duc/duc.db \
  --to   tkrzw:~/.cache/duc/duc.tkrzw.db
```

**Tokyo Cabinet → SQLite3** (for ad-hoc SQL inspection):

```sh
./migrator \
  --from tokyocabinet:/var/cache/duc/duc.db \
  --to   sqlite3:/tmp/duc-inspect.sqlite
# Then: sqlite3 /tmp/duc-inspect.sqlite "select key from blobs"
```

**LevelDB directory → LMDB single file:**

```sh
./migrator \
  --from leveldb:/var/cache/duc/duc-leveldb/ \
  --to   lmdb:/var/cache/duc/duc.lmdb
```

---

## How It Works

1. The source database is opened **read-only**.
2. A full cursor scan iterates every key-value record in storage order.
3. Each record is written verbatim to the destination database.
4. Both databases are flushed and closed cleanly on completion.

Progress is printed every 10 000 records; the final line reports the total
count and any write errors.

Because the copy is raw (below the duc abstraction layer), the destination
database is immediately usable by duc without re-indexing.

---

## Caveats

- **`duc_db_version`** is copied as-is.  Backends that do not normally store
  this key (LevelDB, SQLite3, LMDB) will have it present after migration,
  which is harmless.  Backends that validate it on open (Tokyo Cabinet, Kyoto
  Cabinet, Tkrzw) will accept it as long as the version string matches the
  compiled duc version.

- **LevelDB** stores its data in a directory; make sure the destination
  directory either does not exist or is empty before migrating into it.

- **LMDB** pre-allocates a large virtual address range (1 GB on 32-bit, 256 GB
  on 64-bit).  Actual disk usage is much smaller; the reservation is virtual
  memory only.

- The migrator does **not** validate the integrity of the source database
  before copying.  Run `duc info` on the source first if in doubt.
