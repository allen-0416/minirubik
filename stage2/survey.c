/* survey -- build a pattern database for any compatible abstraction and
 * measure the heuristic it gives against the exact distances (host only)
 *
 * usage: ./survey SPEC[+SPEC...]         detailed report; + combines by max
 *        ./survey --summary SPEC...      one summary line per argument
 *        ./survey --subsets FAM K        summary line for every K-cubie set
 *        ./survey --combos N SPEC...     every N-table max-combination of the
 *                                        listed specs (N = 2 or 3) that fits
 *                                        the budget at 4 bits per entry,
 *                                        sorted by mean h at d = 11
 *
 * SPEC is one of
 *   A          all orientations               (729 abstract states)
 *   B          all positions (permutation)    (5,040)
 *   P          permutation parity             (2)
 *   C:cubies   positions of the listed cubies,             e.g. C:0123
 *   D:cubies   positions and orientations of the cubies,   e.g. D:0123
 *   E:cubies   all orientations + positions of the cubies, e.g. E:01
 *   F:cubies   all positions + orientations of the cubies, e.g. F:01
 *   FULL       the whole state (validation only: h must equal d)
 * Cubies are digits 0..6, the values stored in state_t.p.
 */
#include "oracle_lib.h"

enum { MAX_SPECS = 8, NONE = UINT32_MAX };

typedef struct {
    char name[16];
    char fam;
    int k;
    uint8_t cub[CUBIES];
    uint32_t space;    /* size of the sparse key range */
    uint32_t distinct; /* abstract states actually reached */
    uint8_t *pdb;      /* pdb[key] = abstract distance, 0xFF if unused */
    unsigned diameter;
} spec_t;

static uint32_t key_of(const spec_t *sp, const state_t *s)
{
    uint8_t pos[CUBIES];
    uint32_t key = 0, orank = 0;
    for (int i = 0; i < CUBIES; ++i)
        pos[s->p[i]] = (uint8_t) i;
    for (int i = 0; i < 6; ++i)
        orank = orank * 3 + s->o[i];
    switch (sp->fam) {
    case 'A':
        return orank;
    case 'B':
        return rank_state(s) / ORIENTATIONS;
    case 'P': {
        unsigned inv = 0;
        for (int i = 0; i < CUBIES; ++i)
            for (int j = i + 1; j < CUBIES; ++j)
                inv += s->p[i] > s->p[j];
        return inv & 1U;
    }
    case 'C':
        for (int j = 0; j < sp->k; ++j)
            key = key * 7 + pos[sp->cub[j]];
        return key;
    case 'D':
        for (int j = 0; j < sp->k; ++j)
            key = key * 21 + pos[sp->cub[j]] * 3U + s->o[pos[sp->cub[j]]];
        return key;
    case 'E':
        for (int j = 0; j < sp->k; ++j)
            key = key * 7 + pos[sp->cub[j]];
        return key * ORIENTATIONS + orank;
    case 'F':
        for (int j = 0; j < sp->k; ++j)
            key = key * 3 + s->o[pos[sp->cub[j]]];
        return key * PERMUTATIONS + rank_state(s) / ORIENTATIONS;
    default: /* FULL */
        return rank_state(s);
    }
}

static int parse_spec(const char *text, spec_t *sp)
{
    memset(sp, 0, sizeof *sp);
    snprintf(sp->name, sizeof sp->name, "%s", text);
    if (!strcmp(text, "FULL")) {
        sp->fam = 'X';
        sp->space = STATES;
        return 0;
    }
    sp->fam = text[0];
    if (strchr("ABP", sp->fam) && text[1] == '\0') {
        sp->space = sp->fam == 'A' ? ORIENTATIONS
                    : sp->fam == 'B' ? PERMUTATIONS
                                     : 2;
        return 0;
    }
    if (!strchr("CDEF", sp->fam) || text[1] != ':')
        return 1;
    uint8_t used = 0;
    for (const char *c = text + 2; *c; ++c) {
        int v = *c - '0';
        if (v < 0 || v >= CUBIES || (used >> v & 1) || sp->k == CUBIES)
            return 1;
        used |= (uint8_t) (1U << v);
        sp->cub[sp->k++] = (uint8_t) v;
    }
    if (sp->k == 0)
        return 1;
    uint64_t space = 1;
    for (int j = 0; j < sp->k; ++j)
        space *= sp->fam == 'C' || sp->fam == 'E' ? 7
                 : sp->fam == 'D'                ? 21
                                                 : 3;
    space *= sp->fam == 'E' ? ORIENTATIONS : sp->fam == 'F' ? PERMUTATIONS : 1;
    if (space > STATES)
        return 1;
    sp->space = (uint32_t) space;
    return 0;
}

/* BFS over abstract states. Each abstract state keeps one representative
 * concrete state; compatibility (checked separately by ./compat) makes the
 * abstract successor independent of which representative is used.
 */
static int build_pdb(spec_t *sp)
{
    uint32_t *rep = malloc((size_t) sp->space * sizeof *rep);
    uint32_t *queue = malloc((size_t) sp->space * sizeof *queue);
    sp->pdb = malloc(sp->space);
    if (!rep || !queue || !sp->pdb)
        return 1;
    memset(rep, 0xFF, (size_t) sp->space * sizeof *rep);
    memset(sp->pdb, 0xFF, sp->space);
    state_t s;
    sp->distinct = 0;
    for (uint32_t rank = 0; rank < STATES; ++rank) {
        unrank_state(rank, &s);
        uint32_t key = key_of(sp, &s);
        if (rep[key] == NONE) {
            rep[key] = rank;
            ++sp->distinct;
        }
    }
    state_t solved = {{0, 1, 2, 3, 4, 5, 6}, {0}};
    uint32_t head = 0, tail = 0, start = key_of(sp, &solved);
    sp->pdb[start] = 0;
    queue[tail++] = start;
    sp->diameter = 0;
    while (head < tail) {
        uint32_t key = queue[head++];
        unrank_state(rep[key], &s);
        for (uint8_t m = 0; m < MOVES; ++m) {
            state_t t = apply_move(s, m);
            uint32_t next = key_of(sp, &t);
            if (sp->pdb[next] == UINT8_MAX) {
                sp->pdb[next] = (uint8_t) (sp->pdb[key] + 1);
                if (sp->pdb[next] > sp->diameter)
                    sp->diameter = sp->pdb[next];
                queue[tail++] = next;
            }
        }
    }
    free(rep);
    free(queue);
    return tail == sp->distinct ? 0 : 1;
}

typedef struct {
    uint64_t violations, exact, count[12], sum_h[12], gap[12];
    unsigned min_h11;
} stats_t;

static void measure(spec_t *sp, int n, const uint8_t *dist, stats_t *st)
{
    memset(st, 0, sizeof *st);
    st->min_h11 = 99;
    state_t s;
    for (uint32_t rank = 0; rank < STATES; ++rank) {
        unrank_state(rank, &s);
        unsigned h = 0, d = dist[rank];
        for (int i = 0; i < n; ++i) {
            unsigned v = sp[i].pdb[key_of(&sp[i], &s)];
            if (v > h)
                h = v;
        }
        if (h > d) {
            ++st->violations;
            continue;
        }
        ++st->count[d];
        st->sum_h[d] += h;
        ++st->gap[d - h];
        st->exact += h == d;
        if (d == 11 && h < st->min_h11)
            st->min_h11 = h;
    }
}

static uint64_t entries(const spec_t *sp, int n)
{
    uint64_t e = 0;
    for (int i = 0; i < n; ++i)
        e += sp[i].distinct;
    return e;
}

static double mean_h(const stats_t *st)
{
    uint64_t sum = 0;
    for (int d = 0; d <= 11; ++d)
        sum += st->sum_h[d];
    return (double) sum / (double) STATES;
}

static void summary_header(void)
{
    printf("%-18s %9s %9s %9s %4s %7s %7s %7s %7s %6s\n", "spec", "entries",
           "bytes(1B)", "bytes(4b)", "diam", "mean h", "h@d10", "h@d11",
           "min@11", "viol");
}

static void summary_line(const char *label, spec_t *sp, int n,
                         const stats_t *st)
{
    uint64_t e = entries(sp, n), nib = 0;
    unsigned diam = 0;
    for (int i = 0; i < n; ++i) {
        nib += (sp[i].distinct + 1) / 2;
        if (sp[i].diameter > diam)
            diam = sp[i].diameter;
    }
    printf("%-18s %9llu %9llu %9llu %4u %7.3f %7.3f %7.3f %7u %6llu\n", label,
           (unsigned long long) e, (unsigned long long) e,
           (unsigned long long) nib, diam, mean_h(st),
           (double) st->sum_h[10] / (double) st->count[10],
           (double) st->sum_h[11] / (double) st->count[11], st->min_h11,
           (unsigned long long) st->violations);
}

static void detailed(spec_t *sp, int n, const stats_t *st)
{
    for (int i = 0; i < n; ++i)
        printf("%-10s abstract states %7u (sparse key range %u), diameter "
               "%u, bytes %u (1 B) / %u (4 bit)\n",
               sp[i].name, sp[i].distinct, sp[i].space, sp[i].diameter,
               sp[i].distinct, (sp[i].distinct + 1) / 2);
    printf("\nH1: %llu violations; exact (h == d) on %.2f%% of states; "
           "mean h %.3f\n",
           (unsigned long long) st->violations,
           100.0 * (double) st->exact / (double) STATES, mean_h(st));
    printf("\n    d   states   mean h   mean d-h\n");
    for (int d = 0; d <= 11; ++d)
        printf("%5d  %7llu   %6.3f   %6.3f\n", d,
               (unsigned long long) st->count[d],
               (double) st->sum_h[d] / (double) st->count[d],
               d - (double) st->sum_h[d] / (double) st->count[d]);
    printf("\nd - h    states    share\n");
    for (int k = 0; k <= 11; ++k)
        if (st->gap[k])
            printf("%5d  %8llu  %6.2f%%\n", k, (unsigned long long) st->gap[k],
                   100.0 * (double) st->gap[k] / (double) STATES);
    printf("\nminimum h over the 2,644 distance-11 states: %u\n", st->min_h11);
}

/* Parse "X+Y+..." into sp[]; return the count, or -1. */
static int load_specs(const char *arg, spec_t *sp)
{
    char buf[128];
    int n = 0;
    snprintf(buf, sizeof buf, "%s", arg);
    for (char *tok = strtok(buf, "+"); tok; tok = strtok(NULL, "+")) {
        if (n == MAX_SPECS || parse_spec(tok, &sp[n])) {
            fprintf(stderr, "survey: bad spec '%s'\n", tok);
            return -1;
        }
        if (build_pdb(&sp[n])) {
            fprintf(stderr, "survey: BFS did not reach every abstract state "
                            "of '%s'\n", tok);
            return -1;
        }
        ++n;
    }
    return n;
}

static void free_specs(spec_t *sp, int n)
{
    for (int i = 0; i < n; ++i)
        free(sp[i].pdb);
}

/* Static-data budget left beside the 34,614-byte factored transition tables. */
enum { BUDGET = 131072 - 34614 };

typedef struct {
    char label[64];
    uint64_t bytes1, bytes4;
    double h10, h11, mean;
    unsigned min11;
    uint64_t exact11; /* distance-11 states with h == 11 */
} combo_t;

static int cmp_combo(const void *a, const void *b)
{
    const combo_t *x = a, *y = b;
    if (x->h11 != y->h11)
        return x->h11 < y->h11 ? 1 : -1;
    return (x->min11 < y->min11) - (x->min11 > y->min11);
}

/* Statistics of max(h[idx[0]], ..., h[idx[n_way-1]]); 0 if over budget. */
static int eval_combo(const int idx[], int n_way, uint8_t *const *h,
                      const uint64_t *b1, const uint64_t *b4, char **args,
                      const uint8_t *dist, combo_t *c)
{
    memset(c, 0, sizeof *c);
    c->min11 = 99;
    for (int j = 0; j < n_way; ++j) {
        c->bytes1 += b1[idx[j]];
        c->bytes4 += b4[idx[j]];
        snprintf(c->label + strlen(c->label), sizeof c->label - strlen(c->label),
                 "%s%s", j ? "+" : "", args[idx[j]]);
    }
    if (c->bytes4 > BUDGET)
        return 0;
    uint64_t sum = 0, sum10 = 0, sum11 = 0, n10 = 0, n11 = 0;
    for (uint32_t rank = 0; rank < STATES; ++rank) {
        unsigned v = 0;
        for (int j = 0; j < n_way; ++j)
            if (h[idx[j]][rank] > v)
                v = h[idx[j]][rank];
        sum += v;
        if (dist[rank] == 10) {
            sum10 += v;
            ++n10;
        } else if (dist[rank] == 11) {
            sum11 += v;
            ++n11;
            c->exact11 += v == 11;
            if (v < c->min11)
                c->min11 = v;
        }
    }
    c->mean = (double) sum / (double) STATES;
    c->h10 = (double) sum10 / (double) n10;
    c->h11 = (double) sum11 / (double) n11;
    return 1;
}

/* h[i][rank] for every listed spec, then max over each N-subset. */
static int run_combos(int n_way, int count, char **args, const uint8_t *dist)
{
    uint8_t **h = calloc((size_t) count, sizeof *h);
    uint64_t *b1 = calloc((size_t) count, sizeof *b1);
    uint64_t *b4 = calloc((size_t) count, sizeof *b4);
    if (!h || !b1 || !b4)
        return 1;
    for (int i = 0; i < count; ++i) {
        spec_t sp;
        if (strchr(args[i], '+') || parse_spec(args[i], &sp) ||
            build_pdb(&sp)) {
            fprintf(stderr, "survey: bad spec '%s'\n", args[i]);
            return 1;
        }
        h[i] = malloc(STATES);
        if (!h[i])
            return 1;
        state_t s;
        for (uint32_t rank = 0; rank < STATES; ++rank) {
            unrank_state(rank, &s);
            h[i][rank] = sp.pdb[key_of(&sp, &s)];
        }
        b1[i] = sp.distinct;
        b4[i] = (sp.distinct + 1) / 2;
        free(sp.pdb);
    }
    size_t cap = 1024, n = 0;
    combo_t *out = malloc(cap * sizeof *out);
    for (int i = 0; i < count; ++i)
        for (int j = i + 1; j < count; ++j)
            /* For pairs, k takes the single placeholder value count. */
            for (int k = n_way == 3 ? j + 1 : count;
                 k < count || (n_way == 2 && k == count); ++k) {
                int idx[3] = {i, j, k};
                combo_t c;
                if (eval_combo(idx, n_way, h, b1, b4, args, dist, &c)) {
                    if (n == cap)
                        out = realloc(out, (cap *= 2) * sizeof *out);
                    out[n++] = c;
                }
                if (n_way == 2)
                    break;
            }
    qsort(out, n, sizeof *out, cmp_combo);
    printf("%-30s %9s %9s %4s %4s %7s %7s %7s %7s %6s\n", "combination",
           "bytes(1B)", "bytes(4b)", "1B?", "4b?", "mean h", "h@d10",
           "h@d11", "min@11", "h=11");
    for (size_t i = 0; i < n; ++i)
        printf("%-30s %9llu %9llu %4s %4s %7.3f %7.3f %7.3f %7u %6llu\n",
               out[i].label, (unsigned long long) out[i].bytes1,
               (unsigned long long) out[i].bytes4,
               out[i].bytes1 <= BUDGET ? "yes" : "no", "yes", out[i].mean,
               out[i].h10, out[i].h11, out[i].min11,
               (unsigned long long) out[i].exact11);
    printf("%zu combinations fit the %d-byte budget at 4 bits per entry\n", n,
           BUDGET);
    for (int i = 0; i < count; ++i)
        free(h[i]);
    free(h);
    free(b1);
    free(b4);
    free(out);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fputs("usage: survey SPEC[+SPEC...] | --summary SPEC... | "
              "--subsets FAM K | --combos {2|3} SPEC...\n",
              stderr);
        return 2;
    }
    uint8_t *dist = oracle_distances();
    if (!dist) {
        fputs("survey: could not build the oracle\n", stderr);
        return 1;
    }
    spec_t sp[MAX_SPECS];
    stats_t st;
    int rc = 0;
    if (!strcmp(argv[1], "--summary")) {
        summary_header();
        for (int a = 2; a < argc; ++a) {
            int n = load_specs(argv[a], sp);
            if (n < 0)
                return 1;
            measure(sp, n, dist, &st);
            summary_line(argv[a], sp, n, &st);
            rc |= st.violations != 0;
            free_specs(sp, n);
        }
    } else if (!strcmp(argv[1], "--combos") && argc >= 5 &&
               (argv[2][0] == '2' || argv[2][0] == '3') && !argv[2][1]) {
        rc = run_combos(argv[2][0] - '0', argc - 3, argv + 3, dist);
    } else if (!strcmp(argv[1], "--subsets") && argc == 4) {
        char fam = argv[2][0];
        int k = atoi(argv[3]);
        summary_header();
        for (unsigned mask = 0; mask < 1U << CUBIES; ++mask) {
            if (__builtin_popcount(mask) != k)
                continue;
            char text[16] = {fam, ':'};
            int len = 2;
            for (int c = 0; c < CUBIES; ++c)
                if (mask >> c & 1)
                    text[len++] = (char) ('0' + c);
            text[len] = '\0';
            int n = load_specs(text, sp);
            if (n < 0)
                return 1;
            measure(sp, n, dist, &st);
            summary_line(text, sp, n, &st);
            rc |= st.violations != 0;
            free_specs(sp, n);
        }
    } else {
        int n = load_specs(argv[1], sp);
        if (n < 0)
            return 1;
        measure(sp, n, dist, &st);
        detailed(sp, n, &st);
        rc = st.violations != 0;
        free_specs(sp, n);
    }
    free(dist);
    return rc;
}
