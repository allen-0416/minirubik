/* candidate.h -- the interface check.c expects from a design under test
 *
 * A candidate is one .c file, selected with `make check CAND=file.c`. It is
 * included after oracle_lib.h, so state_t, MOVES, move_names, apply_move,
 * rank_state, and the other solver.c helpers are in scope. It must define:
 *
 *   int cand_init(void);
 *       Build or load every table the design uses. Return 0 on success.
 *
 *   unsigned cand_h(const state_t *s);
 *       The heuristic value the search uses for s. Checked by `h1`.
 *
 *   int cand_solve(const state_t *s, uint8_t moves[], unsigned max_moves,
 *                  uint64_t *expanded);
 *       Write a solution into moves[] (move numbers 0..8, as in move_names)
 *       and return its length, or -1 on failure. Add to *expanded the number
 *       of nodes whose successors were generated; this is the host proxy for
 *       retired instructions on the target.
 *
 * check.c sets oracle_dist before cand_init so that a test-only candidate
 * can use it; a real design must not.
 */
static const uint8_t *oracle_dist;

static int cand_init(void);
static unsigned cand_h(const state_t *s);
static int cand_solve(const state_t *s, uint8_t moves[], unsigned max_moves,
                      uint64_t *expanded);
