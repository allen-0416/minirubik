/* ex2_tables.c -- 練習 2：不用 unrank 建轉移表
 *
 * 編譯與測試（在 stage3/practice/ 下）：
 *   cc -O2 -std=c99 -Wall -Wextra -Wno-sign-compare ex2_tables.c -o ex2 && ./ex2
 *
 * 先讀 ../../solver.c 的 build_table 前兩個迴圈（L197 附近）。它對每個 rank
 * 呼叫 unrank_state，而 unrank_state 裡有 p / f、p %= f、o % 3、o / 3。
 * 這題要換一個方向：不是「給 rank 求狀態」，而是「照 rank 的順序產生狀態」，
 * 第 k 個產生的狀態 rank 就是 k，不必做任何除法。
 *
 * 動手前先回答：
 *   Q1. Lehmer rank 0, 1, 2, 3 對應的排列各是什麼？（用第 1 題的程式印出來看）
 *       它們之間是什麼順序？
 *   Q2. orientation rank 是 o[0..5] 的三進位值。rank 加 1 時 o[0..5] 怎麼變？
 *       （想像里程表：哪一位先動？什麼時候進位？）
 *   Q3. rank 不包含 o[6]，但 B 轉會把位置 6 的方向搬到別的位置。產生下一個
 *       方向向量時，o[6] 應該是多少？（提示：solver.c 的 unrank_state 最後一行，
 *       以及方向和 ≡ 0 (mod 3) 的不變量）
 *
 * 規則：你寫的函式裡不能出現 *、/、%。
 */
#define main solver_main
#include "../../solver.c"
#undef main

/* ---- 貼上你第 1 題的 my_rank_perm 和 my_rank_ori ---- */

static const uint16_t weight[CUBIES] = {720, 120, 24, 6, 2, 1, 1};

static uint16_t my_rank_perm(const uint8_t p[CUBIES])
{
    uint16_t rank = 0;
    /* TODO：雙層迴圈數逆序對，只用加法累加 weight。 */
    for (uint8_t i = 0; i < CUBIES; ++i){
        for (uint8_t j = (uint8_t) (i + 1U); j < CUBIES; ++j){
            if (p[j] < p[i]){
                rank += weight[i];
            }
        }
    }

    return rank;
}

static uint16_t my_rank_ori(const uint8_t o[CUBIES])
{
    uint16_t rank = 0;
    /* TODO：Horner 法，但 rank * 3 改用 shift 和加法。 */
    for (uint8_t i = 0; i < 6; ++i){
        rank = (rank << 1) + rank + o[i];
    }

    return rank;
}

static uint16_t my_perm_move[3][PERMUTATIONS];
static uint16_t my_ori_move[3][ORIENTATIONS];

/* TODO：把 p 改成字典序的下一個排列；已經是最後一個時回傳 0。 */
static int next_perm(uint8_t p[CUBIES])
{
    int i = CUBIES - 2, j = CUBIES - 1;
    while (i >= 0 && p[i] > p[i + 1]){
        i--;
    }

    if (i < 0){
        return 0;
    }

    while (p[j] < p[i]){
        j--;
    }

    uint8_t temp = p[i];
    p[i] = p[j];
    p[j] = temp;

    for (int a = i + 1, b = CUBIES - 1; a < b; a++, b--){
        temp = p[a];
        p[a] = p[b];
        p[b] = temp;
    }

    return 1;

}

/* TODO：把 o[0..5] 加 1（三進位），再設定 o[6]（Q3）。 */
static void next_ori(uint8_t o[CUBIES])
{
    int i = 5, sum = 0;
    while (i >= 0 && o[i] == 2){
        o[i] = 0;
        i--;
    }

    if ( i >= 0){
        o[i] += 1;
    }

    for (int j = 0; j < 6; j++){
        sum += o[j];
    }

    while (sum >= 3){
        sum -= 3;
    }
    
    o[6] = sum ? 3 - sum : 0;

}

/* TODO：填 my_perm_move 和 my_ori_move。
 * my_perm_move[face][r] = 對排列 rank r 做一次 face 的 quarter turn 後的排列 rank。
 * quarter turn 的規則看 solver.c 的 quarter_turn：result[i] 取自 source[face][i]，
 * 方向再加 twist[face][i]（這裡也不能用 % 3）。
 */
static void build_move_tables(void)
{
    uint8_t p[CUBIES] = {0, 1, 2, 3, 4, 5, 6}, o[CUBIES] = {0};
    
    uint8_t next[CUBIES];
    uint16_t rank = 0;
    do {
        for (uint8_t face = 0; face < 3; ++face) {
            for (uint8_t i = 0; i < CUBIES; ++i)
                next[i] = p[source[face][i]];
            my_perm_move[face][rank] = my_rank_perm(next);
        }
        
        ++rank;
        
    } while (next_perm(p));

    for (rank = 0; rank < ORIENTATIONS; ++rank) {
        for (uint8_t face = 0; face < 3; ++face) {
            for (uint8_t i = 0; i < CUBIES; ++i) {
                uint8_t t = (uint8_t) (o[source[face][i]] + twist[face][i]);
                next[i] =  (t >= 3 ? t - 3U : t);
            }
            my_ori_move[face][rank] = my_rank_ori(next);
        }
        next_ori(o);
    }
}

/* ---- 以下是測試，不用改 ----
 * 標準答案用 solver.c 原本的方法建：unrank_state + quarter_turn + rank_state。
 */
int main(void)
{
    static uint16_t ref_p[3][PERMUTATIONS], ref_o[3][ORIENTATIONS];
    state_t s;
    unsigned bad = 0;
    for (uint32_t r = 0; r < PERMUTATIONS; ++r) {
        unrank_state(r * ORIENTATIONS, &s);
        for (uint8_t f = 0; f < 3; ++f) {
            state_t t = quarter_turn(s, f);
            ref_p[f][r] = (uint16_t) (rank_state(&t) / ORIENTATIONS);
        }
    }
    for (uint32_t r = 0; r < ORIENTATIONS; ++r) {
        unrank_state(r, &s);
        for (uint8_t f = 0; f < 3; ++f) {
            state_t t = quarter_turn(s, f);
            ref_o[f][r] = (uint16_t) (rank_state(&t) % ORIENTATIONS);
        }
    }
    build_move_tables();
    for (uint8_t f = 0; f < 3; ++f) {
        for (uint32_t r = 0; r < PERMUTATIONS; ++r)
            if (my_perm_move[f][r] != ref_p[f][r] && bad++ < 3)
                printf("perm_move[%u][%u]: got %u, want %u\n", f, r,
                       my_perm_move[f][r], ref_p[f][r]);
        for (uint32_t r = 0; r < ORIENTATIONS; ++r)
            if (my_ori_move[f][r] != ref_o[f][r] && bad++ < 6)
                printf("ori_move[%u][%u]: got %u, want %u\n", f, r,
                       my_ori_move[f][r], ref_o[f][r]);
    }
    printf(bad ? "FAIL: %u mismatches\n"
               : "PASS: 3 x (5040 + 729) entries match\n",
           bad);
    return bad != 0;
}
