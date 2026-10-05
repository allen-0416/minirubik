/* check -- host-side gates for a candidate design
 *
 * usage: ./check-NAME h1             H1: h(s) <= d(s) for every state
 *        ./check-NAME d11            solve all 2,644 distance-11 states
 *        ./check-NAME h3 [STEP]      H3: every STEP-th state (default 1 = all)
 *        ./check-NAME one STATE      solve one 14-character state
 *
 * Every returned path is replayed: it must reach solved and have length
 * exactly d(s).
 */
#include "oracle_lib.h"
#include "candidate.h"
#include CANDIDATE

enum { MAX_PATH = 32 };

/* Replay moves from s; 1 if they reach solved in exactly want moves. */
static int path_ok(state_t s, const uint8_t moves[], int len, unsigned want)
{
    if (len < 0 || (unsigned) len != want)
        return 0;
    for (int i = 0; i < len; ++i) {
        if (moves[i] >= MOVES)
            return 0;
        s = apply_move(s, moves[i]);
    }
    return rank_state(&s) == 0;
}

static void print_path(const uint8_t moves[], int len)
{
    for (int i = 0; i < len; ++i)
        printf("%s%s", i ? " " : "", move_names[moves[i]]);
    putchar('\n');
}

static int cmp_u64(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *) a, y = *(const uint64_t *) b;
    return (x > y) - (x < y);
}

static int run_h1(const uint8_t *dist)
{
    uint64_t violations = 0, sum_h[12] = {0}, count[12] = {0};
    uint64_t gap[12] = {0}; /* gap[k] = states with d - h == k */
    unsigned max_h = 0;
    state_t s;
    for (uint32_t rank = 0; rank < STATES; ++rank) {
        unrank_state(rank, &s);
        unsigned h = cand_h(&s), d = dist[rank];
        if (h > d) {
            if (violations < 5)
                printf("VIOLATION rank %u: h = %u > d = %u\n", rank, h, d);
            ++violations;
            continue;
        }
        sum_h[d] += h;
        ++count[d];
        ++gap[d - h];
        if (h > max_h)
            max_h = h;
    }
    printf("H1 %s: %llu violations over %u states; h(solved) = %u; max h = "
           "%u\n",
           violations ? "FAILED" : "passed", (unsigned long long) violations,
           (unsigned) STATES, cand_h(&(state_t){{0, 1, 2, 3, 4, 5, 6}, {0}}),
           max_h);
    printf("\n    d   states   mean h\n");
    for (int d = 0; d <= 11; ++d)
        printf("%5d  %7llu   %6.3f\n", d, (unsigned long long) count[d],
               count[d] ? (double) sum_h[d] / (double) count[d] : 0.0);
    printf("\nd - h   states    share\n");
    for (int k = 0; k <= 11; ++k)
        if (gap[k])
            printf("%5d  %7llu  %6.2f%%\n", k, (unsigned long long) gap[k],
                   100.0 * (double) gap[k] / (double) STATES);
    return violations ? 1 : 0;
}

static int run_solve(const uint8_t *dist, unsigned only_d, uint32_t step)
{
    uint8_t moves[MAX_PATH];
    uint64_t *exp_list = malloc((size_t) STATES * sizeof *exp_list);
    uint64_t n = 0, failures = 0, total = 0, worst = 0;
    uint32_t worst_rank = 0;
    double t0 = seconds_now(), last = t0;
    char str[15];
    if (!exp_list)
        return 1;
    for (uint32_t rank = 0; rank < STATES; rank += step) {
        if (only_d <= 11 && dist[rank] != only_d)
            continue;
        state_t s;
        uint64_t expanded = 0;
        unrank_state(rank, &s);
        int len = cand_solve(&s, moves, MAX_PATH, &expanded);
        if (!path_ok(s, moves, len, dist[rank])) {
            if (failures < 5) {
                rank_to_string(rank, str);
                printf("FAIL %s: d = %u, returned length %d\n", str,
                       dist[rank], len);
            }
            ++failures;
        }
        if (n == 0 || expanded > worst) {
            worst = expanded;
            worst_rank = rank;
        }
        exp_list[n++] = expanded;
        total += expanded;
        double now = seconds_now();
        if (now - last > 10.0) {
            fprintf(stderr, "  ... %llu states, %.0f s\n",
                    (unsigned long long) n, now - t0);
            last = now;
        }
    }
    double elapsed = seconds_now() - t0;
    qsort(exp_list, n, sizeof *exp_list, cmp_u64);
    rank_to_string(worst_rank, str);
    printf("%s: %llu states, %llu failures, %.2f s wall clock\n",
           failures ? "FAILED" : "passed", (unsigned long long) n,
           (unsigned long long) failures, elapsed);
    if (n) {
        printf("expanded nodes per state: min %llu, median %llu, mean %.1f, "
               "max %llu (%s)\n",
               (unsigned long long) exp_list[0],
               (unsigned long long) exp_list[n / 2],
               (double) total / (double) n,
               (unsigned long long) exp_list[n - 1], str);
    }
    free(exp_list);
    return failures ? 1 : 0;
}

static int run_one(const uint8_t *dist, const char *input)
{
    state_t s;
    uint8_t moves[MAX_PATH];
    uint64_t expanded = 0;
    if (!parse_state(input, &s)) {
        fprintf(stderr, "check: invalid state %s\n", input);
        return 2;
    }
    unsigned d = dist[rank_state(&s)];
    int len = cand_solve(&s, moves, MAX_PATH, &expanded);
    printf("d = %u, h = %u, length = %d, expanded = %llu, %s\n", d,
           cand_h(&s), len, (unsigned long long) expanded,
           path_ok(s, moves, len, d) ? "optimal" : "WRONG");
    if (len > 0)
        print_path(moves, len);
    return path_ok(s, moves, len, d) ? 0 : 1;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fputs("usage: check {h1 | d11 | h3 [STEP] | one STATE}\n", stderr);
        return 2;
    }
    double t0 = seconds_now();
    uint8_t *dist = oracle_distances();
    if (!dist) {
        fputs("check: could not build the oracle\n", stderr);
        return 1;
    }
    oracle_dist = dist;
    fprintf(stderr, "oracle ready in %.2f s\n", seconds_now() - t0);
    if (cand_init()) {
        fputs("check: cand_init failed\n", stderr);
        return 1;
    }
    int rc = 2;
    if (!strcmp(argv[1], "h1"))
        rc = run_h1(dist);
    else if (!strcmp(argv[1], "d11"))
        rc = run_solve(dist, 11, 1);
    else if (!strcmp(argv[1], "h3"))
        rc = run_solve(dist, 12, argc > 2 ? (uint32_t) atol(argv[2]) : 1);
    else if (!strcmp(argv[1], "one") && argc > 2)
        rc = run_one(dist, argv[2]);
    else
        fputs("usage: check {h1 | d11 | h3 [STEP] | one STATE}\n", stderr);
    free(dist);
    return rc;
}
