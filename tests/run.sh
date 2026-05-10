#!/bin/zsh

COMPILER="./a.out"
TESTDIR="tests"
OUTDIR=$(mktemp -d "${TMPDIR:-/tmp}/ff-tests.XXXXXX") || exit 1
PASS=0
FAIL=0

trap 'rm -rf "$OUTDIR"' EXIT

# Clean up old test artifacts, but keep .c source cases.
rm -f "$TESTDIR"/*.s(N)
for artifact in "$TESTDIR"/[0-9][0-9][0-9]_*(N); do
    [[ "$artifact" == *.c ]] && continue
    rm -f "$artifact"
done

for src in "$TESTDIR"/*.c(N); do
    name=$(basename "$src" .c)
    expected=$(grep -o 'EXPECT:[[:space:]]*[0-9]*' "$src" | sed 's/EXPECT:[[:space:]]*//')
    [[ -z "$expected" ]] && expected=0

    printf "  %s ... " "$name"

    if ! "$COMPILER" < "$src" > "$OUTDIR/$name.s" 2>/dev/null; then
        echo "FAIL (compilation error)"
        ((FAIL++))
        continue
    fi

    if ! cc "$OUTDIR/$name.s" -o "$OUTDIR/$name" 2>/dev/null; then
        echo "FAIL (assembly error)"
        ((FAIL++))
        continue
    fi

    "$OUTDIR/$name"
    actual=$?

    if [[ "$actual" -eq "$expected" ]]; then
        echo "PASS (returned $actual)"
        ((PASS++))
    else
        echo "FAIL (expected $expected, got $actual)"
        ((FAIL++))
    fi
done

echo ""
echo "$PASS passed, $FAIL failed"

if [[ "$FAIL" -ne 0 ]]; then
    exit 1
fi
