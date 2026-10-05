/* my_pdb.h -- 第 3 題你寫的 build_pdb_scan（從 ex3_pdb.c 原樣搬過來）
 *
 * 用法：在 my_tables.h 之後 #include "my_pdb.h"。
 */
#ifndef MY_PDB_H
#define MY_PDB_H

static uint8_t my_pdb_p[PERMUTATIONS], my_pdb_o[ORIENTATIONS];

/* 回傳 x 經過一次 face quarter turn 後的 rank。is_perm 選擇哪張表。 */
static uint16_t turn_once(int is_perm, uint8_t face, uint16_t x)
{
    return is_perm ? my_perm_move[face][x] : my_ori_move[face][x];
}

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

static void my_init_tables(void)
{
    build_move_tables();
    build_pdb_scan(my_pdb_p, PERMUTATIONS, 1);
    build_pdb_scan(my_pdb_o, ORIENTATIONS, 0);
}

#endif /* MY_PDB_H */
