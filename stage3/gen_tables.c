/* gen_tables -- host generator for the target's read-only tables, and gate H2
 *
 *   ./gen_tables tables.s     write the tables as RV32I assembler data
 *
 * The tables come from the author's own builders in practice/my_tables.h and
 * practice/my_pdb.h. Before writing anything, gate H2 checks every table
 * against solver.c (the baseline, used as the reference):
 *   - every entry is populated (no UINT8_MAX left in a pattern database,
 *     every transition entry in range);
 *   - each transition table is a permutation of its index set whose fourth
 *     power is the identity (R^4 = e);
 *   - the solved entry is right: pdb[0] = 0 and no other entry is 0;
 *   - the maximum of each pattern database equals the deepest BFS level of
 *     its abstraction, recomputed here independently from solver.c.
 * Gate H1 runs next: h(s) = max(pdb_p, pdb_o) <= d(s) for all 3,674,160
 * states, against the exact distances in ../stage2/out/dist.bin.
 * The output uses .section .rodata, so it is meant for the GNU assembler
 * (riscv64-unknown-elf-as) and an ELF loaded with `ripes -t elf`.
 */
#define main solver_main
#include "../solver.c"
#undef main
#include "practice/my_tables.h"
#include "practice/my_pdb.h"

static int fails;

static void gate(int ok, const char *what)
{
    printf("%-55s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}

/* Reference depth of every abstract state, by BFS through solver.c. */
static uint8_t ref_max_depth(int is_perm)
{
    static uint8_t seen[PERMUTATIONS];
    static uint32_t queue[PERMUTATIONS];
    uint32_t n = is_perm ? PERMUTATIONS : ORIENTATIONS, head = 0, tail = 0;
    uint8_t max = 0;
    memset(seen, UINT8_MAX, sizeof seen);
    seen[0] = 0;
    queue[tail++] = 0;
    while (head < tail) {
        uint32_t x = queue[head++];
        state_t s, t;
        unrank_state(is_perm ? x * ORIENTATIONS : x, &s);
        for (uint8_t m = 0; m < MOVES; ++m) {
            t = apply_move(s, m);
            uint32_t r = rank_state(&t);
            uint32_t y = is_perm ? r / ORIENTATIONS : r % ORIENTATIONS;
            if (seen[y] == UINT8_MAX) {
                seen[y] = (uint8_t) (seen[x] + 1);
                if (seen[y] > max)
                    max = seen[y];
                queue[tail++] = y;
            }
        }
    }
    return tail == n ? max : UINT8_MAX;
}

static void check_move(const char *name, const uint16_t *t, uint16_t n)
{
    static uint8_t hit[PERMUTATIONS];
    char what[64];
    for (uint8_t f = 0; f < 3; ++f) {
        const uint16_t *row = t + (size_t) f * n;
        int in_range = 1, bijective = 1, order4 = 1;
        memset(hit, 0, n);
        for (uint16_t x = 0; x < n; ++x) {
            if (row[x] >= n) {
                in_range = 0;
                continue;
            }
            bijective &= !hit[row[x]]++;
            uint16_t y = x;
            for (int k = 0; k < 4; ++k)
                y = y < n ? row[y] : y;
            order4 &= y == x;
        }
        snprintf(what, sizeof what, "H2 %s[%u]: in range, bijective, ^4 = id",
                 name, f);
        gate(in_range && bijective && order4, what);
    }
}

static void check_pdb(const char *name, const uint8_t *pdb, uint16_t n,
                      int is_perm)
{
    char what[64];
    uint8_t max = 0;
    int full = 1, zero_only_solved = pdb[0] == 0;
    for (uint16_t x = 0; x < n; ++x) {
        full &= pdb[x] != UINT8_MAX;
        if (x && pdb[x] == 0)
            zero_only_solved = 0;
        if (pdb[x] != UINT8_MAX && pdb[x] > max)
            max = pdb[x];
    }
    uint8_t want = ref_max_depth(is_perm);
    snprintf(what, sizeof what, "H2 %s: all %u entries populated", name, n);
    gate(full, what);
    snprintf(what, sizeof what, "H2 %s: solved entry 0, no other 0", name);
    gate(zero_only_solved, what);
    snprintf(what, sizeof what, "H2 %s: max %u = reference BFS depth %u", name,
             max, want);
    gate(max == want, what);
}

/* H1: admissibility of max(pdb_p, pdb_o) over every state. */
static void check_h1(void)
{
    static uint8_t dist[STATES];
    uint32_t bad = 0, count[12] = {0};
    uint64_t sum[12] = {0};
    FILE *f = fopen("../stage2/out/dist.bin", "rb");
    if (!f || fread(dist, 1, STATES, f) != STATES) {
        gate(0, "H1: read ../stage2/out/dist.bin");
        if (f)
            fclose(f);
        return;
    }
    fclose(f);
    for (uint32_t r = 0; r < STATES; ++r) {
        uint8_t a = my_pdb_p[r / ORIENTATIONS], b = my_pdb_o[r % ORIENTATIONS];
        uint8_t h = a > b ? a : b, d = dist[r];
        if (h > d)
            ++bad;
        if (d < 12) {
            sum[d] += h;
            ++count[d];
        }
    }
    printf("H1 mean h at d = 11: %.3f over %u states\n",
           (double) sum[11] / count[11], count[11]);
    gate(bad == 0, "H1: h <= d over all 3,674,160 states");
}

static void emit_half(FILE *f, const char *label, const uint16_t *v, size_t n)
{
    fprintf(f, "    .balign 2\n%s:\n", label);
    for (size_t i = 0; i < n; ++i)
        fprintf(f, "%s%u%s", i % 12 ? ", " : "    .half ", v[i],
                i % 12 == 11 || i + 1 == n ? "\n" : "");
}

static void emit_byte(FILE *f, const char *label, const uint8_t *v, size_t n)
{
    fprintf(f, "%s:\n", label);
    for (size_t i = 0; i < n; ++i)
        fprintf(f, "%s%u%s", i % 24 ? ", " : "    .byte ", v[i],
                i % 24 == 23 || i + 1 == n ? "\n" : "");
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fputs("usage: gen_tables OUT.s\n", stderr);
        return 2;
    }
    my_init_tables();
    check_move("perm_move", &my_perm_move[0][0], PERMUTATIONS);
    check_move("ori_move", &my_ori_move[0][0], ORIENTATIONS);
    check_pdb("pdb_p", my_pdb_p, PERMUTATIONS, 1);
    check_pdb("pdb_o", my_pdb_o, ORIENTATIONS, 0);
    check_h1();
    if (fails) {
        printf("H1/H2 FAILED (%d checks); %s not written\n", fails,
               argv[1]);
        return 1;
    }
    FILE *f = fopen(argv[1], "w");
    if (!f)
        return 1;
    fprintf(f, "# Generated by stage3/gen_tables from practice/my_tables.h "
               "and practice/my_pdb.h.\n# Do not edit. Gates H1 and H2 passed.\n"
               "# perm_move/ori_move: [face][rank], face 0..2 = R B D, one "
               "quarter turn.\n    .section .rodata\n    .globl perm_move, "
               "ori_move, pdb_p, pdb_o\n");
    emit_half(f, "perm_move", &my_perm_move[0][0], 3 * PERMUTATIONS);
    emit_half(f, "ori_move", &my_ori_move[0][0], 3 * ORIENTATIONS);
    emit_byte(f, "pdb_p", my_pdb_p, PERMUTATIONS);
    emit_byte(f, "pdb_o", my_pdb_o, ORIENTATIONS);
    if (fclose(f))
        return 1;
    printf("H1 and H2 passed; wrote %s (%u B of tables)\n", argv[1],
           (unsigned) (sizeof my_perm_move + sizeof my_ori_move +
                       sizeof my_pdb_p + sizeof my_pdb_o));
    return 0;
}
