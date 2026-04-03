# duc — multi-backend testing

This directory contains scripts for building and cross-testing `duc` across all supported database backends.

## Backends

| Backend        | Binary               | DB file / directory        |
|----------------|----------------------|----------------------------|
| tkrzw          | `duc-tkrzw`          | `*.db`                     |
| tokyocabinet   | `duc-tokyocabinet`   | `*.db`                     |
| sqlite3        | `duc-sqlite3`        | `*.db`                     |
| lmdb           | `duc-lmdb`           | `*.db`                     |
| leveldb        | `duc-leveldb`        | `*.dir/` (directory)       |
| kyotocabinet   | `duc-kyotocabinet`   | `*.db`                     |

## Scripts

### `build-all-backends.sh`

Builds a separate `duc-<backend>` binary for every supported database backend.

**Must be run from the `testing/` directory.**

```bash
cd testing
bash build-all-backends.sh
```

- Runs `autoreconf -i` if `configure` is missing or older than `configure.ac`.
- For each backend: runs `./configure --with-db-backend=<backend>`, then `make`.
- Copies the resulting binary to `testing/duc-<backend>`.
- Saves full build output to `testing/build-<backend>.log`.
- Exits with a non-zero status if any backend fails to build.

The number of parallel make jobs can be controlled via the `JOBS` environment
variable (defaults to `nproc`):

```bash
JOBS=4 bash build-all-backends.sh
```

### `test-compare-backends.sh`

Indexes a filesystem path with every available `duc-<backend>` binary, dumps
the result as JSON, and performs a pairwise comparison to verify that all
backends produce identical output.

```bash
bash test-compare-backends.sh [PATH]
```

- `PATH` defaults to `/usr/share/doc` if not specified.
- Skips any backend whose binary is not present in `testing/`.
- Database files are written to `testing/dbs/` and **kept after the run** for
  further inspection.
- Exits with a non-zero status if any pair of backends produces different JSON.

## Dependencies

The following development libraries must be installed before building:

```bash
sudo apt-get install \
    libtokyocabinet-dev \
    libkyotocabinet-dev \
    libleveldb-dev \
    liblmdb-dev \
    libsqlite3-dev \
    libtkrzw-dev
```
