#!/usr/bin/env bash
#
# test-compare-backends.sh — Index a path with every duc backend and compare JSON output.
#
# For each duc-<backend> binary found in the same directory, this script:
#   1. Indexes the given path into a persistent database in testing/dbs/.
#   2. Dumps the database content as JSON.
#   3. Performs a pairwise diff of all JSON outputs and reports any differences.
#
# Database files and the JSON outputs are kept in testing/dbs/ after the run for further inspection.
#
# Usage:
#   bash test-compare-backends.sh [PATH]
#
# Arguments:
#   PATH  — filesystem path to index (default: /usr/share/doc)
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
INDEX_PATH="${1:-/usr/share/doc}"

BACKENDS=(tkrzw tokyocabinet sqlite3 lmdb leveldb kyotocabinet)
DBDIR="$SCRIPT_DIR/dbs"

mkdir -p "$DBDIR"

echo "Indexing path: $INDEX_PATH"
echo "DB dir:        $DBDIR"
echo ""

# Index and dump JSON for each backend
for backend in "${BACKENDS[@]}"; do
    bin="$SCRIPT_DIR/duc-$backend"
    if [[ ! -x "$bin" ]]; then
        echo "[$backend] SKIP — binary not found: $bin"
        continue
    fi

    # leveldb uses a directory as DB path
    if [[ "$backend" == "leveldb" ]]; then
        db="$DBDIR/$backend.dir"
    else
        db="$DBDIR/$backend.db"
    fi

    json_file="$DBDIR/$backend.json"

    rm -rf "$db"

    echo -n "[$backend] indexing ... "
    if "$bin" index -q -d "$db" "$INDEX_PATH" 2>&1; then
        echo -n "done. dumping json ... "
        "$bin" json -d "$db" "$INDEX_PATH" > "$json_file" 2>&1
        echo "done. ($(wc -c < "$json_file") bytes)"
    else
        echo "FAILED"
        continue
    fi
done

echo ""
echo "=== Pairwise JSON comparison ==="
echo ""

# Collect successfully produced JSON files
successful=()
for backend in "${BACKENDS[@]}"; do
    f="$DBDIR/$backend.json"
    [[ -s "$f" ]] && successful+=("$backend")
done

if [[ ${#successful[@]} -lt 2 ]]; then
    echo "Need at least 2 successful backends to compare."
    exit 1
fi

all_match=true
for ((i = 0; i < ${#successful[@]}; i++)); do
    for ((j = i + 1; j < ${#successful[@]}; j++)); do
        a="${successful[$i]}"
        b="${successful[$j]}"
        fa="$DBDIR/$a.json"
        fb="$DBDIR/$b.json"
        if diff -q "$fa" "$fb" > /dev/null 2>&1; then
            echo "  $a == $b  [identical]"
        else
            echo "  $a != $b  [DIFFER]"
            all_match=false
            diff --unified=3 "$fa" "$fb" | head -40 || true
            echo "  ..."
        fi
    done
done

# Remove lock files left behind by backends (e.g. lmdb creates a .db-lock)
rm -f "$DBDIR"/*.lock "$DBDIR"/*.db-lock

echo ""
if $all_match; then
    echo "Result: all backends produce identical JSON output."
else
    echo "Result: differences found between backends (see above)."
    exit 1
fi
