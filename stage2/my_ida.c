/* my_ida.c -- IDA* candidate (author's own design)
 *
 * Build and test:
 *   make check CAND=my_ida.c
 *   ./check-my_ida one 12345671111111    solved state: expect 0 moves
 *   ./check-my_ida h3 100000             sample of all states
 *   ./check-my_ida d11                   all 2,644 distance-11 states
 */

static uint8_t pdb_o[ORIENTATIONS], pdb_p[PERMUTATIONS];

/* is_perm = 0：建方向表（A）；is_perm = 1：建排列表（B） */
static void build_pdb(uint8_t pdb[], uint32_t size, int is_perm)
{
    uint16_t queue[PERMUTATIONS];          /* 5040 夠放兩種表 */
    uint32_t head = 0, tail = 0;
    memset(pdb, UINT8_MAX, size);          /* 全部標成「還沒拜訪」 */
    pdb[0] = 0;                    /* 起點：已解開 */
    queue[tail++] = 0;
    while (head < tail) {
        uint32_t x = queue[head++];
        state_t s;
        unrank_state(is_perm ? x * ORIENTATIONS : x, &s);      /* 抽象狀態 x → 一個代表狀態 */
        for (uint8_t m = 0; m < MOVES; ++m) {
            state_t t = apply_move(s, m);
            uint32_t r = rank_state(&t);
            uint32_t y = is_perm ? r / ORIENTATIONS : r % ORIENTATIONS;        /* 新狀態 → 抽象狀態 y */
            if (pdb[y] == UINT8_MAX) {     /* 第一次看到 y */
                pdb[y] = pdb[x] + 1;        /* 距離 = x 的距離 + 1 */
                queue[tail++] = y;
            }
        }
    }
}

static int cand_init(void)
{
    build_pdb(pdb_o, ORIENTATIONS, 0);
    build_pdb(pdb_p, PERMUTATIONS, 1);
    return 0;
}

static unsigned cand_h(const state_t *s)
{
    uint32_t r = rank_state(s);
    unsigned a = pdb_o[r % ORIENTATIONS];         /* A：只看方向 */
    unsigned b = pdb_p[r / ORIENTATIONS];         /* B：只看排列 */
    return (a > b) ? a : b;                      /* 兩者取 max */
}

static int dfs(state_t s, unsigned g, unsigned bound, uint8_t last_face,
               uint8_t path[], uint64_t *expanded)
{
    unsigned f = g + cand_h(&s);
    if (f > bound)
        return 0;               /* 剪枝：這一輪不可能在 bound 內解完 */
    if (rank_state(&s) == 0)
        return 1;               /* 找到解 */
    ++*expanded;
    for (uint8_t m = 0; m < MOVES; ++m) {
        if (m / 3 == last_face)
            continue;           /* 同一面：跳過 */
        state_t next = apply_move(s, m);
        path[g] = m;
        if (dfs(next, g + 1, bound, m / 3, path, expanded))
            return 1;
    }
    return 0;                   /* 9 種 move 都失敗 */
}

static int cand_solve(const state_t *s, uint8_t moves[], unsigned max_moves,
                      uint64_t *expanded)
{
    for (unsigned bound = cand_h(s); bound <= max_moves; ++bound)
        if (dfs(*s, 0, bound, 3, moves, expanded))
            return bound;
    return -1;
}
