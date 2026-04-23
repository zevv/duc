#!/usr/bin/env bash
#
# Copyright (c) 2026 George Ruinelli <caco3@ruinelli.ch>
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
#   bash test-migrator.sh [--include-tkrzw-as-source] [PATH]
#
# Arguments:
#   PATH  — filesystem path that was indexed (default: /usr/share/doc)
#           Must match the path used when running test-compare-backends.sh.
#
# Options:
#   --include-tkrzw-as-source
#           Also migrate FROM the tkrzw database.  Disabled by default because
#           tkrzw source iteration is extremely slow (several minutes per
#           destination).  tkrzw is always available as a migration destination.
#
# Environment:
#   TIMEOUT  — seconds allowed per migration before it is killed (default: 300)
#
# Requirements:
#   - ../migrator/migrator must be built  (cd ../migrator && make)
#   - Source databases and JSON files must exist in dbs/
#     (run test-compare-backends.sh first)
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

INCLUDE_TKRZW_SOURCE=0
POSITIONAL=()
for _arg in "$@"; do
    case "$_arg" in
        --include-tkrzw-as-source) INCLUDE_TKRZW_SOURCE=1 ;;
        *) POSITIONAL+=("$_arg") ;;
    esac
done
INDEX_PATH="${POSITIONAL[0]:-/usr/share/doc}"
DBDIR="$SCRIPT_DIR/dbs"
OUTDIR="$DBDIR/migrated"
MIGRATOR="$SCRIPT_DIR/../migrator/migrator"
LOGDIR="$OUTDIR/logs"
TIMEOUT="${TIMEOUT:-300}"

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

BACKENDS=(tokyocabinet kyotocabinet sqlite3 lmdb leveldb tkrzw)

migrate_failed=()
migrate_ok=()
skipped=()
json_failed=()
json_ok=()
diff_fail=()
diff_ok=()

# ============================================================
# Phase 1 — Migrate all databases
# ============================================================
echo "=== Phase 1: Migrate ==="
echo ""

for src in "${BACKENDS[@]}"; do
    src_path="${DB_PATH[$src]}"
    if [[ ! -e "$src_path" ]]; then
        echo "  [$src] SKIP — source DB not found: $src_path"
        skipped+=("$src")
        continue
    fi

    if [[ "$src" == "tkrzw" ]]; then
        if [[ "$INCLUDE_TKRZW_SOURCE" != "1" ]]; then
            echo "  [tkrzw] SKIP as source (pass --include-tkrzw-as-source to enable; iteration is very slow)"
            skipped+=("tkrzw-as-source")
            continue
        fi
        echo "  [tkrzw] WARNING: tkrzw source iteration is very slow — this may take several minutes per destination"
    fi

    for dst in "${BACKENDS[@]}"; do
        [[ "$src" == "$dst" ]] && continue

        dst_ext="${DB_EXT[$dst]}"
        out_path="$OUTDIR/${src}-to-${dst}.${dst_ext}"
        log="$LOGDIR/${src}-to-${dst}.log"

        rm -rf "$out_path"

        printf "  %-14s -> %-14s ... " "$src" "$dst"
        if timeout "$TIMEOUT" "$MIGRATOR" --from "${src}:${src_path}" --to "${dst}:${out_path}" > "$log" 2>&1; then
            echo "ok"
            migrate_ok+=("${src}-to-${dst}")
        else
            rc=$?
            [[ $rc -eq 124 ]] && echo "TIMEOUT (>${TIMEOUT}s)" || echo "FAILED (rc=$rc)"
            migrate_failed+=("${src}-to-${dst}")
        fi
    done
done

# ============================================================
# Phase 2 — Export each migrated database to JSON
# ============================================================
echo ""
echo "=== Phase 2: Export JSON ==="
echo ""

for pair in "${migrate_ok[@]}"; do
    src="${pair%%-to-*}"
    dst="${pair##*-to-}"
    dst_ext="${DB_EXT[$dst]}"
    out_path="$OUTDIR/${pair}.${dst_ext}"
    migrated_json="$OUTDIR/${pair}.json"
    dst_bin="$SCRIPT_DIR/duc-$dst"

    printf "  %-30s ... " "$pair"
    if [[ ! -x "$dst_bin" ]]; then
        echo "SKIP (duc-$dst not found)"
        continue
    fi
    if "$dst_bin" json -d "$out_path" "$INDEX_PATH" > "$migrated_json" 2>&1; then
        echo "ok  ($(wc -c < "$migrated_json") bytes)"
        json_ok+=("$pair")
    else
        echo "FAILED"
        json_failed+=("$pair")
    fi
done

# ============================================================
# Phase 3 — Compare each migrated JSON against source JSON
# ============================================================
echo ""
echo "=== Phase 3: Compare JSON ==="
echo ""

for pair in "${json_ok[@]}"; do
    src="${pair%%-to-*}"
    src_json="$DBDIR/${src}.json"
    migrated_json="$OUTDIR/${pair}.json"

    printf "  %-30s ... " "$pair"
    if [[ ! -s "$src_json" ]]; then
        echo "SKIP (no source JSON for $src)"
        continue
    fi
    if diff -q "$src_json" "$migrated_json" > /dev/null 2>&1; then
        echo "match"
        diff_ok+=("$pair")
    else
        echo "DIFFER"
        diff_fail+=("$pair")
    fi
done

# ============================================================
# Summary
# ============================================================
echo ""
echo "=== Summary ==="
total=$(( ${#BACKENDS[@]} * (${#BACKENDS[@]} - 1) ))
skipped_pairs=$(( ${#skipped[@]} * (${#BACKENDS[@]} - 1) ))
attempted=$(( total - skipped_pairs ))
echo "  Migrations possible  : $total"
echo "  Migrations skipped   : $skipped_pairs"
echo "  Migrations attempted : $attempted"
echo "  Migration failed     : ${#migrate_failed[@]}"
echo "  JSON export failed   : ${#json_failed[@]}"
echo "  JSON match           : ${#diff_ok[@]}"
echo "  JSON differ          : ${#diff_fail[@]}"

if [[ ${#diff_fail[@]} -gt 0 ]]; then
    echo ""
    echo "Migrations with JSON differences:"
    for f in "${diff_fail[@]}"; do
        echo "  --- $f ---"
        diff --unified=3 "$DBDIR/${f%%-to-*}.json" "$OUTDIR/$f.json" | head -20 || true
        echo ""
    done
fi

if [[ ${#migrate_failed[@]} -gt 0 ]]; then
    echo ""
    echo "Failed migrations:"
    for f in "${migrate_failed[@]}"; do echo "  $f"; done
fi

if [[ ${#migrate_failed[@]} -gt 0 || ${#diff_fail[@]} -gt 0 || ${#json_failed[@]} -gt 0 ]]; then
    exit 1
fi
