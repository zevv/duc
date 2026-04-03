/*
 * migrator.c - duc database backend converter
 *
 * Copies every raw key-value record from a duc database stored in one backend
 * format into a new database using a different backend.  All six backends are
 * compiled into a single binary (guarded by HAVE_* macros), so any source /
 * destination pairing is possible without multiple build variants.
 *
 * Usage:
 *   ./migrator --from <format>:<path> --to <format>:<path>
 *
 * Supported formats (enabled at compile time via HAVE_* flags):
 *   tokyocabinet, kyotocabinet, leveldb, sqlite3, lmdb, tkrzw
 *
 * The migration is a raw KV copy (below the duc abstraction layer), so every
 * key is transferred verbatim, including duc_db_version and duc_index_reports.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* ============================================================
 * Generic backend interface
 * ============================================================ */

typedef struct {
    const char *name;

    /* Open the database at path.  readonly=1 for source, 0 for destination.
     * Returns an opaque handle on success, NULL on failure. */
    void *(*open)(const char *path, int readonly);

    /* Flush and close. */
    void  (*close)(void *handle);

    /* Write one record.  Returns 0 on success, -1 on error. */
    int   (*put)(void *handle,
                 const void *key, size_t klen,
                 const void *val, size_t vlen);

    /* Iteration ---------------------------------------------------
     * iter_new()  – create an iterator positioned before the first record.
     * iter_next() – advance and fill *key, *val with malloc'd buffers;
     *               caller must free() both.  Returns 1, or 0 when done.
     * iter_free() – destroy the iterator.
     */
    void *(*iter_new)(void *handle);
    int   (*iter_next)(void *iter,
                       void **key, size_t *klen,
                       void **val, size_t *vlen);
    void  (*iter_free)(void *iter);

    /* Return total number of records, or 0 if not cheaply available. */
    size_t (*count)(void *handle);
} backend_ops_t;


/* ============================================================
 * Tokyo Cabinet  (TCBDB – B+ tree)
 * ============================================================ */
#ifdef HAVE_TOKYOCABINET
#include <tcutil.h>
#include <tcbdb.h>

typedef struct { TCBDB *hdb; BDBCUR *cur; } tc_iter_t;

static void *tc_open(const char *path, int readonly)
{
    TCBDB *hdb = tcbdbnew();
    tcbdbtune(hdb, 256, 512, 131072, 9, 11, BDBTLARGE);
    uint32_t mode = readonly
        ? (HDBONOLCK | HDBOREADER)
        : (HDBOWRITER | HDBOCREAT);
    if (!tcbdbopen(hdb, path, mode)) {
        fprintf(stderr, "tokyocabinet: cannot open '%s': %s\n",
                path, tcbdberrmsg(tcbdbecode(hdb)));
        tcbdbdel(hdb);
        return NULL;
    }
    return hdb;
}

static void tc_close(void *h)
{
    tcbdbclose((TCBDB *)h);
    tcbdbdel((TCBDB *)h);
}

static int tc_put(void *h, const void *k, size_t kl, const void *v, size_t vl)
{
    return tcbdbput((TCBDB *)h, k, (int)kl, v, (int)vl) ? 0 : -1;
}

static void *tc_iter_new(void *h)
{
    tc_iter_t *it = malloc(sizeof *it);
    it->hdb = (TCBDB *)h;
    it->cur = tcbdbcurnew(it->hdb);
    tcbdbcurfirst(it->cur);
    return it;
}

static int tc_iter_next(void *iter,
                        void **key, size_t *klen,
                        void **val, size_t *vlen)
{
    tc_iter_t *it = iter;
    int ks, vs;
    /* tcbdbcurkey / tcbdbcurval each return a malloc'd buffer */
    *key = tcbdbcurkey(it->cur, &ks);
    if (!*key) return 0;
    *klen = (size_t)ks;
    *val  = tcbdbcurval(it->cur, &vs);
    *vlen = (size_t)vs;
    tcbdbcurnext(it->cur);
    return 1;
}

static void tc_iter_free(void *iter)
{
    tc_iter_t *it = iter;
    tcbdbcurdel(it->cur);
    free(it);
}

static size_t tc_count(void *h) { return (size_t)tcbdbrnum((TCBDB *)h); }

static const backend_ops_t tc_ops = {
    "tokyocabinet",
    tc_open, tc_close, tc_put,
    tc_iter_new, tc_iter_next, tc_iter_free,
    tc_count
};
#endif /* HAVE_TOKYOCABINET */


/* ============================================================
 * Kyoto Cabinet  (KCT – tree cabinet with compression)
 * ============================================================ */
#ifdef HAVE_KYOTOCABINET
#include <kclangc.h>

typedef struct { KCCUR *cur; } kc_iter_t;

static void *kc_open(const char *path, int readonly)
{
    KCDB *kdb = kcdbnew();
    char fname[4096];
    snprintf(fname, sizeof fname, "%s#type=kct#opts=c", path);
    uint32_t mode = readonly ? KCOREADER : (KCOWRITER | KCOCREATE);
    if (!kcdbopen(kdb, fname, mode)) {
        fprintf(stderr, "kyotocabinet: cannot open '%s'\n", path);
        kcdbdel(kdb);
        return NULL;
    }
    return kdb;
}

static void kc_close(void *h)
{
    kcdbclose((KCDB *)h);
    kcdbdel((KCDB *)h);
}

static int kc_put(void *h, const void *k, size_t kl, const void *v, size_t vl)
{
    return kcdbset((KCDB *)h, k, kl, v, vl) ? 0 : -1;
}

static void *kc_iter_new(void *h)
{
    kc_iter_t *it = malloc(sizeof *it);
    it->cur = kcdbcursor((KCDB *)h);
    kccurjump(it->cur);
    return it;
}

static int kc_iter_next(void *iter,
                        void **key, size_t *klen,
                        void **val, size_t *vlen)
{
    kc_iter_t *it = iter;
    size_t ks, vs;
    const char *vp;
    /* kccurget packs key+value in one allocation; step=1 advances the cursor. */
    char *k = kccurget(it->cur, &ks, &vp, &vs, 1);
    if (!k) return 0;
    *key  = k;
    *klen = ks;
    /* Copy value into a fresh buffer so caller can always call free() on it */
    /* vp points into the same allocation as k (kccurget packs key+value in
     * one buffer); copy the value but do NOT kcfree(vp) — freeing k via
     * the caller's free(*key) releases the whole block. */
    *val  = malloc(vs);
    memcpy(*val, vp, vs);
    *vlen = vs;
    return 1;
}

static void kc_iter_free(void *iter)
{
    kc_iter_t *it = iter;
    kccurdel(it->cur);
    free(it);
}

static size_t kc_count(void *h) { return (size_t)kcdbcount((KCDB *)h); }

static const backend_ops_t kc_ops = {
    "kyotocabinet",
    kc_open, kc_close, kc_put,
    kc_iter_new, kc_iter_next, kc_iter_free,
    kc_count
};
#endif /* HAVE_KYOTOCABINET */


/* ============================================================
 * LevelDB  (SSTable directory, Snappy compression)
 * ============================================================ */
#ifdef HAVE_LEVELDB
#include <leveldb/c.h>

typedef struct {
    leveldb_t              *db;
    leveldb_options_t      *options;
    leveldb_readoptions_t  *roptions;
    leveldb_writeoptions_t *woptions;
} ldb_handle_t;

typedef struct {
    leveldb_iterator_t    *it;
    leveldb_readoptions_t *roptions;
} ldb_iter_t;

static void *ldb_open(const char *path, int readonly)
{
    ldb_handle_t *h = malloc(sizeof *h);
    char *err = NULL;
    h->options  = leveldb_options_create();
    h->roptions = leveldb_readoptions_create();
    h->woptions = leveldb_writeoptions_create();
    leveldb_options_set_create_if_missing(h->options, !readonly);
    leveldb_options_set_compression(h->options, leveldb_snappy_compression);
    h->db = leveldb_open(h->options, path, &err);
    if (err) {
        fprintf(stderr, "leveldb: cannot open '%s': %s\n", path, err);
        leveldb_free(err);
        leveldb_options_destroy(h->options);
        leveldb_readoptions_destroy(h->roptions);
        leveldb_writeoptions_destroy(h->woptions);
        free(h);
        return NULL;
    }
    return h;
}

static void ldb_close(void *handle)
{
    ldb_handle_t *h = handle;
    leveldb_close(h->db);
    leveldb_options_destroy(h->options);
    leveldb_readoptions_destroy(h->roptions);
    leveldb_writeoptions_destroy(h->woptions);
    free(h);
}

static int ldb_put(void *handle,
                   const void *k, size_t kl,
                   const void *v, size_t vl)
{
    ldb_handle_t *h = handle;
    char *err = NULL;
    leveldb_put(h->db, h->woptions, k, kl, v, vl, &err);
    if (err) { leveldb_free(err); return -1; }
    return 0;
}

static void *ldb_iter_new(void *handle)
{
    ldb_handle_t *h = handle;
    ldb_iter_t *it = malloc(sizeof *it);
    it->roptions = leveldb_readoptions_create();
    it->it = leveldb_create_iterator(h->db, it->roptions);
    leveldb_iter_seek_to_first(it->it);
    return it;
}

static int ldb_iter_next(void *iter,
                         void **key, size_t *klen,
                         void **val, size_t *vlen)
{
    ldb_iter_t *it = iter;
    if (!leveldb_iter_valid(it->it)) return 0;
    /* LevelDB returns pointers into its internal buffer; must copy */
    const char *k = leveldb_iter_key(it->it, klen);
    const char *v = leveldb_iter_value(it->it, vlen);
    *key = malloc(*klen);  memcpy(*key, k, *klen);
    *val = malloc(*vlen);  memcpy(*val, v, *vlen);
    leveldb_iter_next(it->it);
    return 1;
}

static void ldb_iter_free(void *iter)
{
    ldb_iter_t *it = iter;
    leveldb_iter_destroy(it->it);
    leveldb_readoptions_destroy(it->roptions);
    free(it);
}

static size_t ldb_count(void *h) { (void)h; return 0; }

static const backend_ops_t ldb_ops = {
    "leveldb",
    ldb_open, ldb_close, ldb_put,
    ldb_iter_new, ldb_iter_next, ldb_iter_free,
    ldb_count
};
#endif /* HAVE_LEVELDB */


/* ============================================================
 * SQLite3  (single-file, table: blobs(key, value))
 * ============================================================ */
#ifdef HAVE_SQLITE3
#include <sqlite3.h>

typedef struct { sqlite3 *s; } sq_handle_t;
typedef struct { sqlite3_stmt *stmt; } sq_iter_t;

static void *sq_open(const char *path, int readonly)
{
    sq_handle_t *h = malloc(sizeof *h);
    int flags = readonly
        ? SQLITE_OPEN_READONLY
        : (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    if (sqlite3_open_v2(path, &h->s, flags, NULL) != SQLITE_OK) {
        fprintf(stderr, "sqlite3: cannot open '%s': %s\n",
                path, sqlite3_errmsg(h->s));
        sqlite3_close(h->s);
        free(h);
        return NULL;
    }
    if (!readonly) {
        sqlite3_exec(h->s,
            "create table if not exists blobs"
            "(key unique primary key, value)", 0, 0, 0);
        sqlite3_exec(h->s,
            "create index if not exists keys on blobs(key)", 0, 0, 0);
        sqlite3_exec(h->s, "begin", 0, 0, 0);
    }
    return h;
}

static void sq_close(void *handle)
{
    sq_handle_t *h = handle;
    sqlite3_exec(h->s, "commit", 0, 0, 0);
    sqlite3_close(h->s);
    free(h);
}

static int sq_put(void *handle,
                  const void *k, size_t kl,
                  const void *v, size_t vl)
{
    sq_handle_t *h = handle;
    sqlite3_stmt *stmt;
    sqlite3_prepare(h->s,
        "insert or replace into blobs(key,value) values(?,?)",
        -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, k, (int)kl, SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 2, v, (int)vl, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return 0;
}

static void *sq_iter_new(void *handle)
{
    sq_handle_t *h = handle;
    sq_iter_t *it = malloc(sizeof *it);
    sqlite3_prepare_v2(h->s,
        "select key, value from blobs",
        -1, &it->stmt, 0);
    return it;
}

static int sq_iter_next(void *iter,
                        void **key, size_t *klen,
                        void **val, size_t *vlen)
{
    sq_iter_t *it = iter;
    if (sqlite3_step(it->stmt) != SQLITE_ROW) return 0;
    /* Pointers are only valid until the next sqlite3_step(); copy them */
    *klen = (size_t)sqlite3_column_bytes(it->stmt, 0);
    *vlen = (size_t)sqlite3_column_bytes(it->stmt, 1);
    *key = malloc(*klen);
    memcpy(*key, sqlite3_column_blob(it->stmt, 0), *klen);
    *val = malloc(*vlen);
    memcpy(*val, sqlite3_column_blob(it->stmt, 1), *vlen);
    return 1;
}

static void sq_iter_free(void *iter)
{
    sq_iter_t *it = iter;
    sqlite3_finalize(it->stmt);
    free(it);
}

static size_t sq_count(void *h)
{
    sq_handle_t *sh = h;
    sqlite3_stmt *stmt;
    size_t n = 0;
    if (sqlite3_prepare_v2(sh->s, "select count(*) from blobs", -1, &stmt, 0) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) n = (size_t)sqlite3_column_int64(stmt, 0);
        sqlite3_finalize(stmt);
    }
    return n;
}

static const backend_ops_t sq_ops = {
    "sqlite3",
    sq_open, sq_close, sq_put,
    sq_iter_new, sq_iter_next, sq_iter_free,
    sq_count
};
#endif /* HAVE_SQLITE3 */


/* ============================================================
 * LMDB  (memory-mapped single file, MDB_NOSUBDIR)
 * ============================================================ */
#ifdef HAVE_LMDB
#include <lmdb.h>

typedef struct { MDB_env *env; MDB_dbi dbi; MDB_txn *txn; } mdb_handle_t;
typedef struct { MDB_cursor *cur; int started; } mdb_iter_t;

static void *mdb_be_open(const char *path, int readonly)
{
    mdb_handle_t *h = malloc(sizeof *h);
    unsigned int env_flags  = MDB_NOSUBDIR;
    unsigned int open_flags = 0;
    unsigned int txn_flags  = 0;

    /* Virtual map: 1 GB on 32-bit, 256 GB on 64-bit */
    size_t map_size = 1024u * 1024u * 1024u;
    if (sizeof(size_t) == 8) map_size *= 256u;

    if (readonly) {
        env_flags |= MDB_RDONLY;
        txn_flags |= MDB_RDONLY;
    } else {
        open_flags |= MDB_CREATE;
    }

    int rc;
    if ((rc = mdb_env_create(&h->env))                       != MDB_SUCCESS) goto err;
    if ((rc = mdb_env_set_mapsize(h->env, map_size))         != MDB_SUCCESS) goto err;
    if ((rc = mdb_env_open(h->env, path, env_flags, 0664))   != MDB_SUCCESS) goto err;
    if ((rc = mdb_txn_begin(h->env, NULL, txn_flags, &h->txn)) != MDB_SUCCESS) goto err;
    if ((rc = mdb_open(h->txn, NULL, open_flags, &h->dbi))   != MDB_SUCCESS) goto err;
    return h;
err:
    fprintf(stderr, "lmdb: cannot open '%s': %s\n", path, mdb_strerror(rc));
    mdb_env_close(h->env);
    free(h);
    return NULL;
}

static void mdb_be_close(void *handle)
{
    mdb_handle_t *h = handle;
    mdb_txn_commit(h->txn);
    mdb_dbi_close(h->env, h->dbi);
    mdb_env_close(h->env);
    free(h);
}

static int mdb_be_put(void *handle,
                      const void *k, size_t kl,
                      const void *v, size_t vl)
{
    mdb_handle_t *h = handle;
    MDB_val mk = { kl, (void *)k };
    MDB_val mv = { vl, (void *)v };
    return mdb_put(h->txn, h->dbi, &mk, &mv, 0) == MDB_SUCCESS ? 0 : -1;
}

static void *mdb_iter_new(void *handle)
{
    mdb_handle_t *h = handle;
    mdb_iter_t *it = malloc(sizeof *it);
    mdb_cursor_open(h->txn, h->dbi, &it->cur);
    it->started = 0;
    return it;
}

static int mdb_iter_next(void *iter,
                         void **key, size_t *klen,
                         void **val, size_t *vlen)
{
    mdb_iter_t *it = iter;
    MDB_val mk, mv;
    MDB_cursor_op op = it->started ? MDB_NEXT : MDB_FIRST;
    it->started = 1;
    if (mdb_cursor_get(it->cur, &mk, &mv, op) != MDB_SUCCESS) return 0;
    /* LMDB data lives in the memory-mapped file; copy before txn ends */
    *klen = mk.mv_size;  *key = malloc(mk.mv_size);  memcpy(*key, mk.mv_data, mk.mv_size);
    *vlen = mv.mv_size;  *val = malloc(mv.mv_size);  memcpy(*val, mv.mv_data, mv.mv_size);
    return 1;
}

static void mdb_iter_free(void *iter)
{
    mdb_iter_t *it = iter;
    mdb_cursor_close(it->cur);
    free(it);
}

static size_t mdb_count(void *h)
{
    mdb_handle_t *mh = h;
    MDB_stat st;
    if (mdb_stat(mh->txn, mh->dbi, &st) == MDB_SUCCESS) return (size_t)st.ms_entries;
    return 0;
}

static const backend_ops_t mdb_ops = {
    "lmdb",
    mdb_be_open, mdb_be_close, mdb_be_put,
    mdb_iter_new, mdb_iter_next, mdb_iter_free,
    mdb_count
};
#endif /* HAVE_LMDB */


/* ============================================================
 * Tkrzw  (HashDBM, StdFile; new default in 1.5.0-rc2)
 * ============================================================ */
#ifdef HAVE_TKRZW
#include <tkrzw_langc.h>

static void *tkrzw_be_open(const char *path, int readonly)
{
    TkrzwDBM *hdb = tkrzw_dbm_open(
        path, !readonly,
        "dbm=HashDBM,file=StdFile,num_buckets=131072,record_comp_mode=RECORD_COMP_ZSTD");
    if (!hdb) {
        TkrzwStatus s = tkrzw_get_last_status();
        fprintf(stderr, "tkrzw: cannot open '%s': %s\n", path, s.message);
        return NULL;
    }
    return hdb;
}

static void tkrzw_be_close(void *h)
{
    tkrzw_dbm_close((TkrzwDBM *)h);
}

static int tkrzw_be_put(void *h,
                        const void *k, size_t kl,
                        const void *v, size_t vl)
{
    return tkrzw_dbm_set((TkrzwDBM *)h,
                         k, (int32_t)kl,
                         v, (int32_t)vl,
                         1 /* overwrite */) ? 0 : -1;
}

typedef struct { TkrzwDBMIter *it; } tkrzw_iter_t;

static void *tkrzw_iter_new(void *h)
{
    tkrzw_iter_t *it = malloc(sizeof *it);
    it->it = tkrzw_dbm_make_iterator((TkrzwDBM *)h);
    tkrzw_dbm_iter_first(it->it);
    return it;
}

static int tkrzw_iter_next(void *iter,
                           void **key, size_t *klen,
                           void **val, size_t *vlen)
{
    tkrzw_iter_t *it = iter;
    int32_t ks, vs;
    char *k, *v;
    /* tkrzw_dbm_iter_get returns malloc'd key and value; step separately */
    if (!tkrzw_dbm_iter_get(it->it, &k, &ks, &v, &vs)) return 0;
    *key  = k;  *klen = (size_t)ks;
    *val  = v;  *vlen = (size_t)vs;
    tkrzw_dbm_iter_next(it->it);
    return 1;
}

static void tkrzw_iter_free(void *iter)
{
    tkrzw_iter_t *it = iter;
    tkrzw_dbm_iter_free(it->it);
    free(it);
}

static size_t tkrzw_count(void *h) { return (size_t)tkrzw_dbm_count((TkrzwDBM *)h); }

static const backend_ops_t tkrzw_ops = {
    "tkrzw",
    tkrzw_be_open, tkrzw_be_close, tkrzw_be_put,
    tkrzw_iter_new, tkrzw_iter_next, tkrzw_iter_free,
    tkrzw_count
};
#endif /* HAVE_TKRZW */


/* ============================================================
 * Backend registry
 * ============================================================ */

static const backend_ops_t * const backends[] = {
#ifdef HAVE_TOKYOCABINET
    &tc_ops,
#endif
#ifdef HAVE_KYOTOCABINET
    &kc_ops,
#endif
#ifdef HAVE_LEVELDB
    &ldb_ops,
#endif
#ifdef HAVE_SQLITE3
    &sq_ops,
#endif
#ifdef HAVE_LMDB
    &mdb_ops,
#endif
#ifdef HAVE_TKRZW
    &tkrzw_ops,
#endif
    NULL
};

static const backend_ops_t *find_backend(const char *name)
{
    for (int i = 0; backends[i]; i++)
        if (strcmp(backends[i]->name, name) == 0)
            return backends[i];
    return NULL;
}

static void list_backends(FILE *out)
{
    fprintf(out, "Compiled-in backends:");
    for (int i = 0; backends[i]; i++)
        fprintf(out, "  %s", backends[i]->name);
    fprintf(out, "\n");
}


/* ============================================================
 * Argument parsing helpers
 * ============================================================ */

static void usage(const char *argv0)
{
    fprintf(stderr,
        "Usage: %s --from <format>:<path> --to <format>:<path>\n\n"
        "Copies every key-value record from a duc database in one backend\n"
        "format to a new database using a different backend.  The migration\n"
        "is a raw KV copy (below the duc abstraction layer), so all internal\n"
        "duc keys (duc_db_version, duc_index_reports, …) are transferred too.\n\n",
        argv0);
    list_backends(stderr);
}

/* Split "format:path" on the first colon.
 * Writes the format name into fmt (NUL-terminated) and sets *path.
 * Returns 0 on success, -1 if no colon is present. */
static int split_spec(const char *arg,
                      char *fmt, size_t fmtsz,
                      const char **path)
{
    const char *colon = strchr(arg, ':');
    if (!colon) return -1;
    size_t flen = (size_t)(colon - arg);
    if (flen >= fmtsz) flen = fmtsz - 1;
    memcpy(fmt, arg, flen);
    fmt[flen] = '\0';
    *path = colon + 1;
    return 0;
}


/* ============================================================
 * main
 * ============================================================ */

int main(int argc, char **argv)
{
    const char *from_arg = NULL;
    const char *to_arg   = NULL;

    for (int i = 1; i < argc; i++) {
        if      (!strcmp(argv[i], "--from") && i + 1 < argc) from_arg = argv[++i];
        else if (!strcmp(argv[i], "--to")   && i + 1 < argc) to_arg   = argv[++i];
        else    { usage(argv[0]); return 1; }
    }

    if (!from_arg || !to_arg) { usage(argv[0]); return 1; }

    char from_fmt[64], to_fmt[64];
    const char *from_path, *to_path;

    if (split_spec(from_arg, from_fmt, sizeof from_fmt, &from_path) < 0) {
        fprintf(stderr, "error: --from argument must be <format>:<path>\n");
        return 1;
    }
    if (split_spec(to_arg, to_fmt, sizeof to_fmt, &to_path) < 0) {
        fprintf(stderr, "error: --to argument must be <format>:<path>\n");
        return 1;
    }

    const backend_ops_t *src_ops = find_backend(from_fmt);
    const backend_ops_t *dst_ops = find_backend(to_fmt);

    if (!src_ops) {
        fprintf(stderr, "error: unknown source backend '%s'\n", from_fmt);
        list_backends(stderr);
        return 1;
    }
    if (!dst_ops) {
        fprintf(stderr, "error: unknown destination backend '%s'\n", to_fmt);
        list_backends(stderr);
        return 1;
    }

    fprintf(stderr, "Migrating: %s:%s  ->  %s:%s\n",
            from_fmt, from_path, to_fmt, to_path);

    void *src = src_ops->open(from_path, 1 /* read-only */);
    if (!src) return 1;

    void *dst = dst_ops->open(to_path,   0 /* read-write */);
    if (!dst) { src_ops->close(src); return 1; }

    size_t total = src_ops->count(src);

    fprintf(stderr, "Scanning...");
    fflush(stderr);
    void *iter = src_ops->iter_new(src);
    fprintf(stderr, "\r");
    fflush(stderr);

    void *key, *val;
    size_t klen, vlen;
    unsigned long done = 0, errors = 0;
    const int BAR = 40;

    while (src_ops->iter_next(iter, &key, &klen, &val, &vlen)) {
        if (dst_ops->put(dst, key, klen, val, vlen) != 0)
            errors++;
        free(key);
        free(val);
        done++;
        if (done % 100 == 0 || done == 1) {
            if (total > 0) {
                int filled = (int)((double)done / total * BAR);
                fprintf(stderr, "\r  [");
                for (int i = 0; i < BAR; i++)
                    fputc(i < filled ? '=' : (i == filled ? '>' : ' '), stderr);
                fprintf(stderr, "] %lu/%zu (%d%%)",
                        done, total, (int)((double)done / total * 100));
            } else {
                fprintf(stderr, "\r  %lu records", done);
            }
            fflush(stderr);
        }
    }

    /* Final completed bar */
    if (total > 0) {
        fprintf(stderr, "\r  [");
        for (int i = 0; i < BAR; i++) fputc('=', stderr);
        fprintf(stderr, "] %lu/%lu (100%%)\n", done, done);
    } else {
        fprintf(stderr, "\r  %lu records\n", done);
    }

    src_ops->iter_free(iter);
    src_ops->close(src);
    dst_ops->close(dst);

    if (errors)
        fprintf(stderr, "  %lu write error(s)\n", errors);
    fprintf(stderr, "Done: %lu records copied.\n", done);

    return errors ? 1 : 0;
}
