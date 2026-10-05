/* ex3_pdb.c -- 練習 3：用轉移表建 pattern database
 *
 * 編譯與測試（在 stage3/practice/ 下）：
 *   cc -O1 -g -std=c99 -Wall -Wextra -Wno-sign-compare \
 *      -fsanitize=address,undefined ex3_pdb.c -o ex3 && ./ex3
 *
 * 出發點是你 Stage 2 的 build_pdb（../../stage2/my_ida.c）。它對每個抽象狀態
 * 呼叫 unrank_state、apply_move、rank_state，再用 / 或 % 729 取出一半的 rank。
 * 第 2 題的轉移表已經直接給出「轉一次之後的 rank」，所以這些全部可以拿掉。
 *
 * 動手前先回答：
 *   Q1. 舊版 BFS 對每個 x 試 9 個 move（m = 0..8）。用 my_perm_move[face][x]
 *       只能轉 1 次（quarter turn）。怎麼得到 R2、R'？
 *       （提示：solver.c build_table 的 next_p = permutation[face][next_p]）
 *   Q2. 舊版的 queue 有 5,040 格 uint16_t = 10,080 B。BFS 時，pdb[x] 本身就
 *       記錄了「x 在第幾層」。如果不用 queue，怎麼找出「第 d 層的所有 x」？
 *       代價是什麼？
 *
 * 規則：不能用 unrank_state、apply_move、rank_state，也不能用 *、/、%。
 */
#define main solver_main
#include "../../solver.c"
#undef main
#include "my_tables.h"

static uint8_t my_pdb_p[PERMUTATIONS], my_pdb_o[ORIENTATIONS];

/* 回傳 x 經過一次 face quarter turn 後的 rank。is_perm 選擇哪張表。 */
static uint16_t turn_once(int is_perm, uint8_t face, uint16_t x)
{
    return is_perm ? my_perm_move[face][x] : my_ori_move[face][x];
}

/* TODO A：有 queue 的 BFS。結構照你 Stage 2 的 build_pdb，
 * 但鄰居改用 turn_once 產生（Q1）。 */
static void build_pdb_queue(uint8_t pdb[], uint16_t n, int is_perm)
{
    for (uint16_t x = 0; x < n; ++x){
        pdb[x] = UINT8_MAX;
    }

    uint16_t queue[n];
    uint16_t head = 0, tail = 0;
    queue[tail++] = 0;
    pdb[0] = 0;

    while (head < tail) {
        uint16_t x = queue[head++];
        for (uint8_t face = 0; face < 3; ++face){
            uint16_t y = x;
            for (uint8_t turn = 0; turn < 3; ++turn){
                y = turn_once(is_perm, face, y);
                if (pdb[y] == UINT8_MAX){
                    pdb[y] = pdb[x] + 1;
                    queue[tail++] = y;
                }
            }
        }
    }
}

/* TODO B（挑戰）：不用 queue 的 BFS（Q2）。結果要和 A 完全一樣。 */
static void build_pdb_scan(uint8_t pdb[], uint16_t n, int is_perm)
{
    for (uint16_t x = 0; x < n; ++x){
        pdb[x] = UINT8_MAX;
    }

    pdb[0] = 0;
    uint8_t depth = 0;
    int visit = 1;

    while (visit) {
        visit = 0;
        for (uint16_t x = 0; x < n; ++x){
            if (pdb[x] != depth){
                continue;
            }
            for (uint8_t face = 0; face < 3; ++face){
                uint16_t y = x;
                for (uint8_t turn = 0; turn < 3; ++turn){
                    y = turn_once(is_perm, face, y);
                    if (pdb[y] == UINT8_MAX){
                        pdb[y] = depth + 1;
                        visit = 1;
                    }
                }
            }
        }
        depth++;
    }
}

/* ---- 以下是測試，不用改 ----
 * 標準答案：你 Stage 2 的 build_pdb，原樣搬過來。
 */
static void ref_build_pdb(uint8_t pdb[], uint32_t size, int is_perm)
{
    uint16_t queue[PERMUTATIONS];
    uint32_t head = 0, tail = 0;
    memset(pdb, UINT8_MAX, size);
    pdb[0] = 0;
    queue[tail++] = 0;
    while (head < tail) {
        uint32_t x = queue[head++];
        state_t s;
        unrank_state(is_perm ? x * ORIENTATIONS : x, &s);
        for (uint8_t m = 0; m < MOVES; ++m) {
            state_t t = apply_move(s, m);
            uint32_t r = rank_state(&t);
            uint32_t y = is_perm ? r / ORIENTATIONS : r % ORIENTATIONS;
            if (pdb[y] == UINT8_MAX) {
                pdb[y] = pdb[x] + 1;
                queue[tail++] = y;
            }
        }
    }
}

static unsigned compare(const char *name, const uint8_t *got,
                        const uint8_t *want, uint16_t n)
{
    unsigned bad = 0;
    for (uint16_t x = 0; x < n; ++x)
        if (got[x] != want[x] && bad++ < 3)
            printf("%s[%u]: got %u, want %u\n", name, x, got[x], want[x]);
    return bad;
}

int main(void)
{
    static uint8_t ref_p[PERMUTATIONS], ref_o[ORIENTATIONS];
    unsigned bad_a = 0, bad_b = 0;
    ref_build_pdb(ref_p, PERMUTATIONS, 1);
    ref_build_pdb(ref_o, ORIENTATIONS, 0);
    build_move_tables();

    build_pdb_queue(my_pdb_p, PERMUTATIONS, 1);
    build_pdb_queue(my_pdb_o, ORIENTATIONS, 0);
    bad_a += compare("A pdb_p", my_pdb_p, ref_p, PERMUTATIONS);
    bad_a += compare("A pdb_o", my_pdb_o, ref_o, ORIENTATIONS);
    printf(bad_a ? "A (queue): FAIL, %u mismatches\n"
                 : "A (queue): PASS\n", bad_a);

    memset(my_pdb_p, 0, sizeof my_pdb_p);
    memset(my_pdb_o, 0, sizeof my_pdb_o);
    build_pdb_scan(my_pdb_p, PERMUTATIONS, 1);
    build_pdb_scan(my_pdb_o, ORIENTATIONS, 0);
    bad_b += compare("B pdb_p", my_pdb_p, ref_p, PERMUTATIONS);
    bad_b += compare("B pdb_o", my_pdb_o, ref_o, ORIENTATIONS);
    printf(bad_b ? "B (scan):  FAIL, %u mismatches\n"
                 : "B (scan):  PASS\n", bad_b);
    return bad_a || bad_b;
}
