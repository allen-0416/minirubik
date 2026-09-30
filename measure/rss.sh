#!/bin/bash
# rss.sh -- measure peak resident set size of Ripes running a program
#
# usage: ./rss.sh PROGRAM.s PARAM VALUE...
#   e.g. ./rss.sh store_sb.s N_BYTES 16 1048576 2097152 4194304
#
# Runs on RV32_ISS, REPS times per VALUE, pinned to CPU $CPU.
# Output, one line per run:  VALUE MAX_RSS_KB
set -eu
CPU=${CPU:-2}
REPS=${REPS:-3}
prog=$1 param=$2; shift 2
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
for v in "$@"; do
    src="$tmp/$v.s"
    sed "s/^\(\s*\.equ $param,\)\s*[0-9]*/\1 $v/" "$prog" > "$src"
    grep -q "\.equ $param, $v\b" "$src" || { echo "cannot set $param in $prog" >&2; exit 1; }
    for _ in $(seq "$REPS"); do
        rss=$( { QT_QPA_PLATFORM=offscreen taskset -c "$CPU" /usr/bin/time -v \
                 ripes --mode cli -t asm --src "$src" --proc RV32_ISS \
                 > /dev/null; } 2>&1 | awk '/Maximum resident set size/ {print $NF}' )
        echo "$v $rss"
    done
done
