# duc DB Backend Formats

Reference for all database backends supported across duc versions, derived from
the source implementations in `src/libduc/db-*.c` and `configure.ac`.

---

## Tokyo Cabinet (`tokyocabinet`)

- **Introduced:** ≤ 1.4.6  
- **Default in:** 1.4.6  
- **Storage layout:** Single file on disk  
- **Magic header (first bytes):** `ToKyO CaBiNeT`  
- **Internal type:** `TCBDB` — B+ Tree Database  
- **Compression:** Optional deflate (`BDBTDEFLATE`), enabled via `--compress` flag  
- **Tuning:** `tcbdbtune(hdb, 256, 512, 131072, 9, 11, BDBTLARGE [| BDBTDEFLATE])`  
- **Version check:** Stores and validates `duc_db_version` key on open  
- **Notes:**
  - The `BDBTLARGE` flag is always set, allowing the file to exceed 2 GB.
  - `DUC_OPEN_FORCE` triggers `BDBOTRUNC`, which truncates and recreates the file.

---

## Kyoto Cabinet (`kyotocabinet`)

- **Introduced:** ≤ 1.4.6  
- **Default in:** —  
- **Storage layout:** Single file on disk  
- **Magic header (first bytes):** `Kyoto CaBiNeT`  
- **Internal type:** KCT (Tree Cabinet), opened with `#type=kct#opts=c`  
- **Compression:** Enabled unconditionally via `opts=c` in the open string  
- **Version check:** Stores and validates `duc_db_version` key on open  
- **Notes:**
  - Error mapping is incomplete; all backend errors map to `DUC_E_UNKNOWN`.
  - The `DUC_OPEN_COMPRESS` flag is accepted but has no additional effect since
    compression is always on via the open string.

---

## LevelDB (`leveldb`)

- **Introduced:** ≤ 1.4.6  
- **Default in:** —  
- **Storage layout:** **Directory** (not a single file); LevelDB stores multiple
  SSTable (`.ldb`/`.sst`) and manifest files inside a directory.  
- **Magic header:** N/A — detected as a directory by `duc_db_type_check()`  
- **Compression:** Snappy compression is always enabled
  (`leveldb_snappy_compression`); the `DUC_OPEN_COMPRESS` flag has no effect.  
- **Version check:** None — does not store or check `duc_db_version`  
- **Notes:**
  - Because the path is a directory, it behaves differently from all other
    backends when specifying `--database`.
  - `leveldb_options_set_create_if_missing` is always set; the DB is created
    automatically if it does not exist.

---

## SQLite3 (`sqlite3`)

- **Introduced:** ≤ 1.4.6  
- **Default in:** —  
- **Storage layout:** Single file on disk  
- **Magic header (first bytes):** `SQLite format 3`  
- **Internal schema:** Single table `blobs(key UNIQUE PRIMARY KEY, value)` with
  an additional index `keys` on the `key` column.  
- **Compression:** None — no compression support  
- **Version check:** None — does not store or check `duc_db_version`  
- **Notes:**
  - All writes are batched inside a single `BEGIN`/`COMMIT` transaction that
    spans the lifetime of the open database (committed on `db_close`).
  - On open, a deliberate bogus query (`select bogus from bogus`) is run to
    detect corrupt files that `sqlite3_open()` would otherwise accept silently.
  - `insert or replace` semantics are used, so re-indexing a path overwrites the
    previous entry cleanly.

---

## LMDB (`lmdb`)

- **Introduced:** ≤ 1.4.6  
- **Default in:** —  
- **Storage layout:** Single file on disk (opened with `MDB_NOSUBDIR`)  
- **Magic header:** Standard LMDB file header (not checked by duc's type
  detector; falls through to `unknown`)  
- **Compression:** None — no compression support  
- **Version check:** None — does not store or check `duc_db_version`  
- **Memory map size:**
  - 32-bit platforms: 1 GB (`1024 * 1024 * 1024`)
  - 64-bit platforms: 256 GB (`1024 * 1024 * 1024 * 256`)
- **Notes:**
  - Uses a single write transaction (`MDB_txn`) for all puts, committed on
    `db_close`. A write error in `db_put` calls `exit(1)` immediately.
  - The large pre-allocated map size is a virtual address reservation only;
    actual disk usage grows on demand.

---

## Tkrzw (`tkrzw`)

- **Introduced:** 1.5.0-rc2  
- **Default in:** 1.5.0-rc2  
- **Storage layout:** Single file on disk  
- **Magic header:** Tkrzw-specific header (not yet checked by duc's type
  detector)  
- **Internal type:** `HashDBM` with `StdFile` file driver  
- **Base open options:** `dbm=HashDBM,file=StdFile,offset_width=5`  
- **Compression:** Optional ZSTD record compression (`record_comp_mode=RECORD_COMP_ZSTD`),
  enabled at compile time via `--with-tkrzw-zstd` and at runtime via the
  `DUC_OPEN_COMPRESS` flag. Falls back to `NONE` if not compiled in.  
- **Version check:** Stores and validates `duc_db_version` key on open  
- **Filesystem size hints:** The `num_buckets` tuning parameter is scaled via
  new `DUC_FS_*` flags:
  | Flag              | `num_buckets`  |
  |-------------------|---------------|
  | `DUC_FS_BIG`      | 100,000,000   |
  | `DUC_FS_BIGGER`   | 1,000,000,000 |
  | `DUC_FS_BIGGEST`  | 10,000,000,000|
- **Notes:**
  - `DUC_OPEN_FORCE` appends `,truncate=true` to the options string, recreating
    the file.
  - Tkrzw is a successor/spiritual replacement for both Tokyo Cabinet and Kyoto
    Cabinet, providing a modern hash-based store with better compression options.

---

## Summary Table

| Backend        | File/Dir | Single file | Compression       | Version key | Default in  |
|----------------|----------|-------------|-------------------|-------------|-------------|
| Tokyo Cabinet  | File     | Yes         | Optional (deflate)| Yes         | 1.4.6       |
| Kyoto Cabinet  | File     | Yes         | Always (kct opts) | Yes         | —           |
| LevelDB        | Dir      | **No**      | Always (Snappy)   | No          | —           |
| SQLite3        | File     | Yes         | None              | No          | —           |
| LMDB           | File     | Yes         | None              | No          | —           |
| Tkrzw          | File     | Yes         | Optional (ZSTD)   | Yes         | 1.5.0-rc2   |
