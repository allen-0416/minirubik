#!/usr/bin/env bash
# remeasure_stage23.sh -- rerun every Stage 2 and Stage 3 measurement
#
#   ./remeasure_stage23.sh            about 9 minutes (H3 takes most of it)
#   FULL_H3=1 ./remeasure_stage23.sh  also Stage 2's full H3 (~70 minutes)
#
# Writes, each overwritten:
#   stage2/data/ida_AB_chain.txt   my_ida.c on the three chain states
#   stage2/data/ida_prune.txt      same, heuristic 0, same-face pruning on
#   stage2/data/ida_noprune.txt    same, heuristic 0, no same-face pruning
#   stage3/data/h1_h2.txt          gates H1 and H2 (gen_tables)
#   stage3/data/h3_own.txt         gate H3 over all 3,674,160 states
#   stage2/data/ida_AB_h3_full.txt (only with FULL_H3=1)
#
# The two variants are the earlier versions of stage2/my_ida.c named in
# stage2_report.md section 10. They are derived from my_ida.c by two sed
# edits at run time; my_ida.c itself is not changed.
set -euo pipefail
cd "$(dirname "$0")"

CHAIN="76352141113232 76543123331132 76543213233233"

echo "== Stage 2: oracle"
make -s -C stage2 oracle

echo "== Stage 2: A + B (my_ida.c) on the chain states"
make -s -C stage2 check CAND=my_ida.c
(cd stage2 && for s in $CHAIN; do ./check-my_ida one "$s"; done) |
    tee stage2/data/ida_AB_chain.txt

# Variant 1: cand_h returns 0 (same-face pruning kept).
# Variant 2: additionally drop the same-face test.
mkdir -p stage2/variants
sed 's/^    return (a > b) ? a : b;.*/    (void) a; (void) b; return 0;  \/* variant: h = 0 *\//' \
    stage2/my_ida.c >stage2/variants/ida_prune.c
sed -e '/if (m \/ 3 == last_face)/,+1d' stage2/variants/ida_prune.c \
    >stage2/variants/ida_noprune.c
grep -q 'variant: h = 0' stage2/variants/ida_prune.c
! grep -q 'last_face)$' stage2/variants/ida_noprune.c

for v in prune noprune; do
    echo "== Stage 2: h = 0, ${v} (variants/ida_${v}.c)"
    make -s -C stage2 check CAND=variants/ida_${v}.c
    (cd stage2 && for s in $CHAIN; do ./check-ida_${v} one "$s"; done) |
        tee stage2/data/ida_${v}.txt
done

if [ "${FULL_H3:-0}" = 1 ]; then
    echo "== Stage 2: full H3 (about 70 minutes)"
    (cd stage2 && ./check-my_ida h3) | tee stage2/data/ida_AB_h3_full.txt
fi

echo "== Stage 3: gates H1 and H2"
make -s -C stage3 h2

echo "== Stage 3: gate H3 over all states (about 7.5 minutes)"
make -s -C stage3 h3

echo "== done; review with: git status --short stage2/data stage3/data"
