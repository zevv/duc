#!/usr/bin/env bash
#
# test_migrate-db-any-to-any.sh — Migrate every duc database in dbs/ to every other backend format.
#
# For each source database found in testing/dbs/ the script invokes the migrator
# binary for every other backend, producing a converted database in
# testing/dbs/migrated/.  Output files are named <src>-to-<dst>.<ext>
# (or <src>-to-<dst>.dir for LevelDB).  Per-migration logs are written to
# testing/dbs/migrated/logs/.
#
# Any existing output file/directory for a given pair is removed before
# migrating so the run is always clean and reproducible.
#
# Usage:
#   bash test_migrate-db-any-to-any.sh
#
# Environment:
#   TIMEOUT  — seconds allowed per migration before it is killed (default: 120)
#
# Requirements:
#   - ../migrator/migrator must be built  (cd ../migrator && make)
#   - Source databases must exist in dbs/ (run test-compare-backends.sh first)
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DBDIR="$SCRIPT_DIR/dbs"
OUTDIR="$DBDIR/migrated"
MIGRATOR="$SCRIPT_DIR/../migrator/migrator"
LOGDIR="$OUTDIR/logs"
TIMEOUT="${TIMEOUT:-120}"

if [[ ! -x "$MIGRATOR" ]]; then
    echo "error: migrator binary not found: $MIGRATOR" >&2
    echo "  cd ../migrator && make" >&2
    exit 1
fi

mkdir -p "$OUTDIR" "$LOGDIR"

# Map each backend to its source path and file extension
declare -A DB_PATH
declare -A DB_EXT
DB_PATH[tkrzw]="$DBDIR/tkrzw.db"
DB_EXT[tkrzw]="db"
DB_PATH[tokyocabinet]="$DBDIR/tokyocabinet.db"
DB_EXT[tokyocabinet]="db"
DB_PATH[sqlite3]="$DBDIR/sqlite3.db"
DB_EXT[sqlite3]="db"
DB_PATH[lmdb]="$DBDIR/lmdb.db"
DB_EXT[lmdb]="db"
DB_PATH[leveldb]="$DBDIR/leveldb.dir"
DB_EXT[leveldb]="dir"
DB_PATH[kyotocabinet]="$DBDIR/kyotocabinet.db"
DB_EXT[kyotocabinet]="db"

BACKENDS=(tkrzw tokyocabinet sqlite3 lmdb leveldb kyotocabinet)

failed=()
skipped=()

for src in "${BACKENDS[@]}"; do
    src_path="${DB_PATH[$src]}"
    if [[ ! -e "$src_path" ]]; then
        echo "[$src] SKIP — source DB not found: $src_path"
        skipped+=("$src:*")
        continue
    fi

    for dst in "${BACKENDS[@]}"; do
        [[ "$src" == "$dst" ]] && continue

        dst_ext="${DB_EXT[$dst]}"
        out_path="$OUTDIR/${src}-to-${dst}.${dst_ext}"

        rm -rf "$out_path"

        log="$LOGDIR/${src}-to-${dst}.log"
        printf "  %-14s -> %-14s ... " "$src" "$dst"
        if timeout "$TIMEOUT" "$MIGRATOR" --from "${src}:${src_path}" --to "${dst}:${out_path}" > "$log" 2>&1; then
            echo "ok"
        else
            rc=$?
            if [[ $rc -eq 124 ]]; then
                echo "TIMEOUT (>${TIMEOUT}s)"
            else
                echo "FAILED (rc=$rc)"
            fi
            failed+=("${src}-to-${dst}")
        fi
    done
done

echo ""
echo "=== Migration summary ==="
total=$(( ${#BACKENDS[@]} * (${#BACKENDS[@]} - 1) ))
echo "  Attempted : $total"
echo "  Failed    : ${#failed[@]}"
echo "  Skipped   : ${#skipped[@]}"

if [[ ${#failed[@]} -gt 0 ]]; then
    echo ""
    echo "Failed migrations:"
    for f in "${failed[@]}"; do echo "  $f"; done
    exit 1
fi
