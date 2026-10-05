/* ex4_ida.c -- 練習 4：帶 rank 的遞迴 IDA*
 *
 * 編譯與測試（在 stage3/practice/ 下）：
 *   cc -O2 -g -std=c99 -Wall -Wextra -Wno-sign-compare ex4_ida.c -o ex4
 *   ./ex4 one 76352141113232     單一狀態（d = 3），印出解和展開數
 *   ./ex4 chain                  三個驗證過的狀態（d = 3, 5, 7）
 *   ./ex4 d11                    全部 2,644 個距離 11 的狀態
 *
 * 需要 ../../stage2/out/dist.bin 和 d11.txt（cd ../../stage2 && make oracle）。
 *
 * 出發點是你 Stage 2 的 dfs 和 cand_solve（../../stage2/my_ida.c）。舊版每個
 * 節點傳一整個 state_t，每次都呼叫 cand_h → rank_state，每個子節點都呼叫
 * apply_move，同面判斷用 m / 3。這題全部換掉：
 *   - 節點只帶兩個數字 (p, o)：排列 rank 和方向 rank；
 *   - 子節點用 my_perm_move / my_ori_move 產生，同一面連續查三次（第 3 題 Q1）；
 *   - 已解判斷：rank_state(&s) == 0 換成什麼？
 *   - 同面判斷：不用 m / 3，用 face 迴圈變數直接比。
 * 遞迴先保留，第 5 題再拿掉。
 *
 * expanded 的定義和 Stage 2 一樣：通過剪枝、又不是已解狀態、要產生子節點的
 * 節點才算一個。這樣你的展開數可以直接和 Stage 2 的數字比：
 *   最難的狀態 54721631111111 應該是 106,635。
 *
 * 規則：搜尋裡不能用 state_t、rank_state、apply_move、*、/、%。
 */
#define main solver_main
#include "../../solver.c"
#undef main
#include "my_tables.h"
#include "my_pdb.h"

static uint64_t expanded;

/* TODO：max(h_B, h_A)，查 my_pdb_p 和 my_pdb_o。 */
static uint8_t my_h(uint16_t p, uint16_t o)
{
    uint8_t h1 = my_pdb_p[p];
    uint8_t h2 = my_pdb_o[o];
    return (h1 > h2 ? h1 : h2);
}

/* TODO：遞迴 IDA*。找到解回傳 1，並把走法寫進 path[0..g-1]。
 * path[k] 的編號和 move_names 一樣：face 0..2，turn 0..2 → R R2 R' B ...
 * 提示：結構照你 Stage 2 的 dfs。move 編號也不能用 face * 3。
 */
static int my_dfs(uint16_t p, uint16_t o, uint8_t g, uint8_t bound,
                  uint8_t last_face, uint8_t path[])
{
    if (g + my_h(p, o) > bound)
        return 0;
    if (p == 0 && o == 0)
        return 1;
    ++expanded;
    for (uint8_t face = 0; face < 3; ++face) {
        if (face == last_face)
            continue;
        uint16_t np = p, no = o;          // 每個 face 從 (p, o) 重新開始
        for (uint8_t turn = 0; turn < 3; ++turn) {
            np = my_perm_move[face][np];
            no = my_ori_move[face][no];
            path[g] = (face << 1U) + face + turn;
            if (my_dfs(np, no, g + 1, bound, face, path))
                return 1;
        }
    }
    return 0;
}

/* TODO：bound 從 my_h 開始，每次 +1，最多到 11。回傳解的長度，失敗回傳 -1。 */
static int my_solve(uint16_t p, uint16_t o, uint8_t path[])
{
    uint8_t bound = my_h(p, o);
    while (bound <= 11) {

        if (my_dfs(p, o, 0, bound, 3, path))  // 3 表示沒有上一個 face
            return bound;
        
        ++bound;
    }   

    return -1;
}

/* ---- 以下是測試，不用改 ---- */
#include <stdlib.h>

static uint8_t dist[STATES];

/* 解一個狀態並驗證：長度等於 d(s)，重播後回到已解。回傳 1 表示正確。 */
static int check(const char *str, uint64_t *nodes, int verbose)
{
    state_t s;
    uint8_t path[32];
    if (!parse_state(str, &s)) {
        printf("bad state %s\n", str);
        return 0;
    }
    uint32_t r = rank_state(&s);
    uint64_t before = expanded;
    int len = my_solve((uint16_t) (r / ORIENTATIONS),
                       (uint16_t) (r % ORIENTATIONS), path);
    *nodes = expanded - before;
    state_t t = s;
    for (int i = 0; i < len; ++i) {
        if (path[i] >= MOVES) {
            len = -2;
            break;
        }
        t = apply_move(t, path[i]);
    }
    int ok = len == dist[r] && rank_state(&t) == 0;
    if (verbose || !ok) {
        printf("%s: d = %u, ", str, dist[r]);
        if (len >= 0) {
            printf("%d moves:", len);
            for (int i = 0; i < len; ++i)
                printf(" %s", move_names[path[i]]);
        } else {
            printf("no solution");
        }
        printf(", %llu expanded, %s\n", (unsigned long long) *nodes,
               ok ? "OK" : "FAIL");
    }
    return ok;
}

int main(int argc, char **argv)
{
    FILE *f = fopen("../../stage2/out/dist.bin", "rb");
    if (!f || fread(dist, 1, STATES, f) != STATES) {
        puts("cannot read ../../stage2/out/dist.bin");
        return 1;
    }
    fclose(f);
    my_init_tables();
    uint64_t nodes;
    if (argc == 3 && !strcmp(argv[1], "one"))
        return !check(argv[2], &nodes, 1);
    if (argc == 2 && !strcmp(argv[1], "chain")) {
        static const char *const s[] = {"12345671111111", "76352141113232",
                                        "76543123331132", "76543213233233",
                                        "54721631111111"};
        int bad = 0;
        for (int i = 0; i < 5; ++i)
            bad += !check(s[i], &nodes, 1);
        return bad != 0;
    }
    if (argc == 2 && !strcmp(argv[1], "d11")) {
        FILE *d = fopen("../../stage2/out/d11.txt", "r");
        char str[16], worst[16] = "";
        unsigned rank, n = 0, bad = 0;
        uint64_t max = 0, sum = 0;
        if (!d)
            return 1;
        while (fscanf(d, "%u %15s", &rank, str) == 2) {
            bad += !check(str, &nodes, 0);
            sum += nodes;
            if (nodes > max) {
                max = nodes;
                strcpy(worst, str);
            }
            ++n;
        }
        fclose(d);
        printf("d11: %u / %u optimal; expanded mean %.0f, max %llu (%s)\n",
               n - bad, n, (double) sum / n, (unsigned long long) max, worst);
        return bad != 0;
    }
    puts("usage: ex4 one STATE | chain | d11");
    return 2;
}
