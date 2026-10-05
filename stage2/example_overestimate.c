/* example_overestimate.c -- harness self-test ONLY, not a design
 *
 * A deliberately inadmissible heuristic (d + 1 on every unsolved state with
 * an odd rank), to show that `h1` reports violations. It provides no solver.
 */
static int cand_init(void)
{
    return oracle_dist ? 0 : 1;
}

static unsigned cand_h(const state_t *s)
{
    uint32_t rank = rank_state(s);
    return oracle_dist[rank] + (rank & 1U);
}

static int cand_solve(const state_t *s, uint8_t moves[], unsigned max_moves,
                      uint64_t *expanded)
{
    (void) s;
    (void) moves;
    (void) max_moves;
    (void) expanded;
    return -1;
}
