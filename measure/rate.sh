#!/bin/bash
# rate.sh -- measure retired instructions, cycles and model execution time
#
# usage: ./rate.sh PROGRAM.s PARAM PROC VALUE...
#   e.g. ./rate.sh counter.s ITERS RV32_ISS 1000000 4000000 16000000
#
# For each VALUE, rewrites ".equ PARAM, ..." in a temporary copy of
# PROGRAM.s and runs it REPS times, pinned to CPU $CPU.
# Output, one line per run:  VALUE IRET CYCLES EXECTIME_MS
set -eu
CPU=${CPU:-2}
REPS=${REPS:-5}
prog=$1 param=$2 proc=$3; shift 3
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
for v in "$@"; do
    src="$tmp/$v.s"
    sed "s/^\(\s*\.equ $param,\)\s*[0-9]*/\1 $v/" "$prog" > "$src"
    grep -q "\.equ $param, $v\b" "$src" || { echo "cannot set $param in $prog" >&2; exit 1; }
    for _ in $(seq "$REPS"); do
        QT_QPA_PLATFORM=offscreen taskset -c "$CPU" ripes --mode cli -t asm \
            --src "$src" --proc "$proc" --iret --cycles --exectime |
        awk -v v="$v" '/instructions retired/ {getline; i=$1}
                       /^===== cycles/        {getline; c=$1}
                       /execution time/       {getline; t=$1}
                       /ERROR|Error/          {print > "/dev/stderr"}
                       END {if (t == "") exit 1; print v, i, c, t}'
    done
done
