/* my_tables.h -- 第 1、2 題你寫的函式（從 ex2_tables.c 原樣搬過來）
 *
 * 用法：在 #include "../../solver.c" 之後 #include "my_tables.h"。
 * 它用到 solver.c 的 CUBIES、PERMUTATIONS、ORIENTATIONS、source、twist。
 */
#ifndef MY_TABLES_H
#define MY_TABLES_H

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


#endif /* MY_TABLES_H */
