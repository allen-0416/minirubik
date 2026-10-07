/* my_tables.h -- ranks and quarter-turn tables without *, /, or %
 *
 * From exercises 1 and 2 (ex1_rank.c, ex2_tables.c). Include after the
 * definitions of CUBIES, PERMUTATIONS, ORIENTATIONS, source, and twist.
 */
#ifndef MY_TABLES_H
#define MY_TABLES_H

static const uint16_t weight[CUBIES] = {720, 120, 24, 6, 2, 1, 1};

static uint16_t my_rank_perm(const uint8_t p[CUBIES])
{
    uint16_t rank = 0;
    /* Lehmer rank: every inversion p[j] < p[i], j > i, adds (6 - i)!. */
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
    /* Base 3 over o[0..5]; rank * 3 = (rank << 1) + rank. */
    for (uint8_t i = 0; i < 6; ++i){
        rank = (rank << 1) + rank + o[i];
    }

    return rank;
}

static uint16_t my_perm_move[3][PERMUTATIONS];
static uint16_t my_ori_move[3][ORIENTATIONS];

/* Lexicographic successor; 0 after the last permutation. The k-th
 * permutation in this order has Lehmer rank k. */
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

/* Base-3 odometer over o[0..5]; o[6] keeps the twist sum 0 mod 3. */
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

/* my_perm_move[f][r] / my_ori_move[f][r]: rank after one quarter turn of
 * face f. States are enumerated in rank order, so nothing is unranked. */
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
