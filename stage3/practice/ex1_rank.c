/* ex1_rank.c -- 練習 1：不用乘法的 rank
 *
 * 編譯與測試（在 stage3/practice/ 下）：
 *   cc -O2 -std=c99 -Wall -Wextra -Wno-sign-compare ex1_rank.c -o ex1 && ./ex1
 *
 * 目標：寫出 my_rank_perm 和 my_rank_ori，結果要和 solver.c 的
 * rank_state 完全一致，但函式裡不能出現 *、/、%。
 *
 * 先讀 ../../solver.c 的 rank_state（L84 附近），回答下面兩個問題再動手：
 *   Q1. p = p * (CUBIES - i) + smaller 跑完 7 輪後，第 i 輪的 smaller
 *       最後被乘上了什麼？（提示：把 i = 0, 1, 2 的展開式寫出來）
 *   Q2. o = o * 3 + d 裡的 o * 3，能用哪些 RV32I 有的運算（加、shift）寫？
 */
#define main solver_main
#include "../../solver.c"
#undef main

/* TODO（Q1 的答案）：每個位置 i 的權重。 */
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

/* ---- 以下是測試，不用改 ---- */
int main(void)
{
    state_t s;
    unsigned bad = 0;
    for (uint32_t r = 0; r < PERMUTATIONS; ++r) {
        unrank_state(r * ORIENTATIONS, &s);
        if (my_rank_perm(s.p) != r && bad++ < 3)
            printf("perm rank %u: got %u\n", r, my_rank_perm(s.p));
    }
    for (uint32_t r = 0; r < ORIENTATIONS; ++r) {
        unrank_state(r, &s);
        if (my_rank_ori(s.o) != r && bad++ < 6)
            printf("ori rank %u: got %u\n", r, my_rank_ori(s.o));
    }
    printf(bad ? "FAIL: %u mismatches\n" : "PASS: 5040 + 729 ranks match\n",
           bad);
    return bad != 0;
}
