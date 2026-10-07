#!/usr/bin/env bash
# d11_sweep.sh -- run every distance-11 state on Ripes and record --iret
#
#   ./d11_sweep.sh [solver|ref] [JOBS] [LIMIT]
#
#   solver  the hand-written assembly (default), from solver.s
#   ref     the gcc -O2 reference build of ../stage3/solver_ida.c
#   JOBS    parallel Ripes processes (default: half the CPUs, at least 1,
#           so the machine stays responsive; every Ripes runs under nice)
#   LIMIT   only the first LIMIT states (default: all 2,644)
#
# The pass condition is a worst case over all 2,644 distance-11 states:
# no state may exceed 5e7 retired instructions on RV32_ISS. For every state
# this checks, inside one Ripes run:
#   - exit code 0 (the program verified its own path by replay, T5);
#   - the printed solution has 11 moves;
#   - the printed solution equals ../stage3/solver_ida (same search order).
# Results: data/d11_<build>.csv (state,iret,exit,moves,same_as_c,output)
# and a summary on stdout and in data/d11_<build>_summary.txt.
#
# Pick one invocation; the lines below are alternatives, not a sequence.
set -euo pipefail
cd "$(dirname "$0")"

BUILD=${1:-solver}
JOBS=${2:-$(( $(nproc) / 2 > 0 ? $(nproc) / 2 : 1 ))}
LIMIT=${3:-0}
PROC=${PROC:-RV32_ISS}
BUDGET=50000000
RV=${RV_PREFIX:-riscv64-unknown-elf-}
LIST=../stage2/out/d11.txt
C_SOLVER=../stage3/solver_ida

case "$BUILD" in
solver) make -s solver.o input.o ../stage3/tables.s ;;
ref) make -s ref.o input.o ../stage3/tables.s ;;
*) echo "usage: $0 [solver|ref] [JOBS] [LIMIT]" >&2; exit 2 ;;
esac
make -s -C ../stage3 solver_ida
test -f "$LIST" || make -s -C ../stage2 oracle
"${RV}as" -march=rv32i -mabi=ilp32 ../stage3/tables.s -o tables.o

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
mkdir -p data
OUT=data/d11_${BUILD}.csv
SUMMARY=data/d11_${BUILD}_summary.txt

# One state: link it with its own input.o, run Ripes, print one CSV row.
run_one() {
    local state=$1 dir
    dir=$(mktemp -d -p "$WORK")
    printf '    .data\n    .globl input\ninput:\n    .string "%s"\n' \
        "$state" >"$dir/in.s"
    "${RV}as" -march=rv32i -mabi=ilp32 "$dir/in.s" -o "$dir/in.o"
    "${RV}ld" --no-relax -m elf32lriscv "$BUILD.o" "$dir/in.o" tables.o -o "$dir/a.elf"
    local log out code iret moves want same
    # Ripes' print-string ecall (a7 = 4, used by the gcc build) also prints
    # the terminating NUL; drop NULs so both builds compare as plain text.
    log=$(nice -n 10 ripes --mode cli -t elf --src "$dir/a.elf" --proc "$PROC" --iret \
        --timeout 600000 2>&1 | tr -d '\000')
    out=$(printf '%s\n' "$log" | head -n 1 | tr -s ' ' | sed 's/ *$//')
    code=$(printf '%s\n' "$log" | awk '/Program exited with code:/{print $NF}')
    iret=$(printf '%s\n' "$log" | awk '/instructions retired/{getline; print $1}')
    moves=$(printf '%s\n' "$out" | wc -w)
    want=$("$C_SOLVER" "$state")
    same=$([ "$out" = "$want" ] && echo 1 || echo 0)
    printf '%s,%s,%s,%s,%s,%s\n' "$state" "${iret:-NA}" "${code:-NA}" \
        "$moves" "$same" "$out"
    rm -rf "$dir"
}
export -f run_one
export WORK BUILD PROC RV C_SOLVER

states=$(awk '{print $2}' "$LIST")
[ "$LIMIT" -gt 0 ] && states=$(printf '%s\n' "$states" | head -n "$LIMIT")
total=$(printf '%s\n' "$states" | wc -l)
echo "running $total distance-11 states, build=$BUILD, proc=$PROC, jobs=$JOBS"
start=$(date +%s)
{
    echo "state,iret,exit,moves,same_as_c,output"
    printf '%s\n' "$states" | xargs -P "$JOBS" -I{} bash -c 'run_one "$@"' _ {} |
        sort -t, -k2,2n
} >"$OUT"
elapsed=$(($(date +%s) - start))

awk -F, -v budget=$BUDGET -v build="$BUILD" -v proc="$PROC" \
    -v elapsed="$elapsed" 'NR > 1 {
        n++; iret = $2 + 0; sum += iret; v[n] = iret
        if (n == 1 || iret < min) min = iret
        if (iret > max) { max = iret; worst = $1 }
        if ($2 == "NA" || $3 != 0) bad_exit++
        if ($4 != 11) bad_len++
        if ($5 != 1) diff_c++
        if (iret > budget) over++
    } END {
        median = v[int((n + 1) / 2)]   # rows are sorted by iret
        printf "build %s on %s: %d distance-11 states, %d s wall clock\n", build, proc, n, elapsed
        printf "retired instructions: min %d, median %d, mean %.0f, max %d (%s)\n", min, median, sum / n, max, worst
        printf "budget 5e7: %d over; worst case uses %.1f%% of it\n", over, 100 * max / budget
        printf "exit code != 0: %d; length != 11: %d; differs from solver_ida: %d\n", bad_exit, bad_len, diff_c
        printf "%s\n", (over || bad_exit || bad_len) ? "FAIL" : "PASS"
    }' "$OUT" | tee "$SUMMARY"
