#!/bin/bash
# refine.sh -- rebuild the first working version of solver.s and measure
#              it against the final one
#
#   ./refine.sh | tee data/refine.txt
#
# v2 is solver.s itself. v1, the first working version, was never committed,
# so it is rebuilt from v2 by undoing the refinement:
#   - the row-offset tables are loaded with `la` for every child again,
#     instead of once into s8 and s9;
#   - the leftover no-op `addi t4, t4, 0` returns before the max in the h
#     computation.
# Both versions are linked with the same tables and input and run on
# RV32_ISS with --iret; their output and exit code must match.
set -eu
cd "$(dirname "$0")"
RV=${RV_PREFIX:-riscv64-unknown-elf-}
AS="${RV}as -march=rv32i -mabi=ilp32"
LD="${RV}ld --no-relax -m elf32lriscv"
STATES="21345671111111 54721631111111"
mkdir -p variants

cp solver.s variants/v2.s
sed -e '/la   s8, perm_row/d' -e '/la   s9, ori_row/d' \
    -e 's/^    add  t6, s8, t4$/    la   t6, perm_row\n    add  t6, t6, t4/' \
    -e 's/^    add  t6, s9, t4$/    la   t6, ori_row\n    add  t6, t6, t4/' \
    -e 's/^    bgeu t4, t5, child_h_done$/    addi t4, t4, 0\n&/' \
    solver.s >variants/v1.s

# Each rewrite must have happened exactly where intended.
[ "$(grep -c 'la   t6, perm_row' variants/v1.s)" = 1 ]
[ "$(grep -c 'la   t6, ori_row' variants/v1.s)" = 1 ]
! grep -q 'la   s8, perm_row' variants/v1.s
[ "$(grep -c 'addi t4, t4, 0' variants/v1.s)" = 1 ]

$AS ../stage3/tables.s -o variants/tables.o
printf '%-4s %6s %8s  %-16s %10s  %s\n' build .text static state iret output
for v in v1 v2; do
    $AS variants/$v.s -o variants/$v.o
    for s in $STATES; do
        make -s input.s STATE=$s >/dev/null
        $AS input.s -o variants/input.o
        $LD variants/$v.o variants/input.o variants/tables.o -o variants/$v.elf
        read -r text data <<<"$(${RV}size -A variants/$v.elf | awk '
            $1 == ".text" { t = $2 } $1 ~ /^\.(rodata|data|bss)/ { d += $2 }
            END { print t, d }')"
        out=$(nice ripes --mode cli -t elf --src variants/$v.elf \
            --proc RV32_ISS --iret --timeout 600000)
        iret=$(awk '/instructions retired/ { getline; print $1 }' <<<"$out")
        code=$(sed -n 's/.*exited with code: //p' <<<"$out")
        sol=$(head -1 <<<"$out")
        printf '%-4s %6s %8s  %-16s %10s  exit %s  %s\n' \
            $v "$text" "$data" $s "$iret" "$code" "$sol"
    done
done
