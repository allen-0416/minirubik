/* example_perfect.c -- harness self-test ONLY, not a design
 *
 * Uses the oracle's complete distance table as the heuristic and walks
 * downhill. This is exactly the precomputed full table the assignment
 * forbids on the target; it exists to show that check.c accepts a correct
 * candidate. Every state should pass h1, d11, and h3 with d(s) expansions.
 */
static int cand_init(void)
{
    return oracle_dist ? 0 : 1;
}

static unsigned cand_h(const state_t *s)
{
    return oracle_dist[rank_state(s)];
}

static int cand_solve(const state_t *s, uint8_t moves[], unsigned max_moves,
                      uint64_t *expanded)
{
    state_t cur = *s;
    unsigned len = 0;
    while (rank_state(&cur) != 0) {
        unsigned here = cand_h(&cur);
        int found = 0;
        ++*expanded;
        for (uint8_t m = 0; m < MOVES && !found; ++m) {
            state_t next = apply_move(cur, m);
            if (cand_h(&next) + 1 == here) {
                if (len == max_moves)
                    return -1;
                moves[len++] = m;
                cur = next;
                found = 1;
            }
        }
        if (!found)
            return -1;
    }
    return (int) len;
}
