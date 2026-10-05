/* Host-only oracle: exact distances derived from the baseline BFS in
 * solver.c. solver.c is included unchanged; its main is renamed so that the
 * tools here can provide their own. Nothing in this file may reach the
 * target: it is the complete distance table the assignment rules out there.
 */
#define _POSIX_C_SOURCE 200809L
#define main solver_main
#include "../solver.c"
#undef main

#include <time.h>

/* dist[rank] = exact HTM distance to solved, for every rank.
 * Each walk follows toward_solved until it meets a state whose distance is
 * already known, then fills the walked prefix backwards.
 */
static uint8_t *oracle_distances(void)
{
    uint8_t diameter;
    uint8_t *table = build_table(&diameter);
    uint8_t *dist = malloc(STATES);
    if (!table || !dist || diameter != 11) {
        free(table);
        free(dist);
        return NULL;
    }
    memset(dist, UINT8_MAX, STATES);
    dist[0] = 0;
    for (uint32_t rank = 0; rank < STATES; ++rank) {
        uint32_t path[12], cur = rank;
        unsigned n = 0;
        state_t s;
        unrank_state(rank, &s);
        while (dist[cur] == UINT8_MAX) {
            path[n++] = cur;
            s = apply_move(s, table[cur]);
            cur = rank_state(&s);
        }
        for (unsigned i = n; i-- > 0;)
            dist[path[i]] = (uint8_t) (dist[cur] + (n - i));
    }
    free(table);
    return dist;
}

/* The 14-character input format parse_state accepts. */
static void rank_to_string(uint32_t rank, char out[15])
{
    state_t s;
    unrank_state(rank, &s);
    for (int i = 0; i < CUBIES; ++i) {
        out[i] = (char) ('1' + s.p[i]);
        out[i + CUBIES] = (char) ('1' + s.o[i]);
    }
    out[14] = '\0';
}

static double seconds_now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double) ts.tv_sec + (double) ts.tv_nsec * 1e-9;
}
