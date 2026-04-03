#!/usr/bin/env bash
#
# Copyright (c) 2026 George Ruinelli <caco3@ruinelli.ch>
#
# build-all-backends.sh — Build duc for every supported database backend.
#
# For each backend (tkrzw, tokyocabinet, sqlite3, lmdb, leveldb, kyotocabinet)
# this script runs ./configure --with-db-backend=<backend>, compiles duc, and
# copies the resulting binary as testing/duc-<backend>.  Build output for each
# backend is saved to testing/build-<backend>.log.
#
# Usage:
#   cd testing && bash build-all-backends.sh
#
# Environment:
#   JOBS  — number of parallel make jobs (default: nproc)
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
JOBS="${JOBS:-$(nproc)}"

# Ensure the script is executed from within the testing/ directory
if [[ "$(pwd)" != "$SCRIPT_DIR" ]]; then
    echo "error: must be run from the testing/ directory" >&2
    echo "  cd $(basename "$SCRIPT_DIR") && bash $(basename "$0")" >&2
    exit 1
fi

BACKENDS=(tkrzw tokyocabinet sqlite3 lmdb leveldb kyotocabinet)

cd "$ROOT_DIR"

# Regenerate build system if configure is missing or older than configure.ac
if [[ ! -f configure || configure.ac -nt configure ]]; then
    echo "==> Running autoreconf -i ..."
    autoreconf -i
fi

failed=()

for backend in "${BACKENDS[@]}"; do
    echo ""
    echo "==> Building duc-$backend ..."

    if ! ./configure --with-db-backend="$backend" > "$SCRIPT_DIR/build-$backend.log" 2>&1; then
        echo "    configure FAILED (see testing/build-$backend.log)"
        failed+=("$backend")
        continue
    fi

    if ! make -j"$JOBS" >> "$SCRIPT_DIR/build-$backend.log" 2>&1; then
        echo "    make FAILED (see testing/build-$backend.log)"
        failed+=("$backend")
        continue
    fi

    cp duc "$SCRIPT_DIR/duc-$backend"
    echo "    -> $SCRIPT_DIR/duc-$backend OK"
done

echo ""
echo "=== Build summary ==="
for backend in "${BACKENDS[@]}"; do
    bin="$SCRIPT_DIR/duc-$backend"
    if [[ " ${failed[*]:-} " == *" $backend "* ]]; then
        echo "  FAIL  $backend"
    elif [[ -x "$bin" ]]; then
        echo "  OK    $backend  ($("$bin" --version 2>&1 | head -1))"
    else
        echo "  MISS  $backend"
    fi
done

if [[ ${#failed[@]} -gt 0 ]]; then
    echo ""
    echo "Some backends failed: ${failed[*]}"
    exit 1
fi
