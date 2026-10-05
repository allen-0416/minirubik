#define main solver_main
#include "../../solver.c"
#undef main
#include "my_tables.h"
#include "my_pdb.h"

static uint64_t expanded;

/* 從第 4 題貼上 my_h。 */
static uint8_t my_h(uint16_t p, uint16_t o)
{
    uint8_t h1 = my_pdb_p[p];
    uint8_t h2 = my_pdb_o[o];
    return (h1 > h2 ? h1 : h2);
}

/* TODO（Q1）：一層需要記住的值。 */
typedef struct {
    uint16_t p, o; /* 這一層的節點 */
    /* TODO：還需要什麼？ */
    uint8_t face, turn; /* 這一層的下一個子節點 */
    uint16_t np, no; /* 這一層的下一個子節點的狀態 */
    uint8_t last_face;
} frame_t;

/* TODO（Q3）：大小。 */
static frame_t stack[11];

/* TODO：一輪 bound 的深度優先搜尋，不用遞迴。
 * 找到解回傳長度（並寫好 path），這一輪找不到回傳 -1。
 * expanded 的計數位置要和第 4 題一樣，展開數才會相同。
 *
 * 建議的流程（每一圈只做一小步）：
 *   1. 根節點放進 stack[0]，g = 0，剪枝和已解檢查做完才 ++expanded。
 *   2. 迴圈：看目前這一層 (stack[g]) 的下一個子節點是什麼
 *        - 這一層的子節點都試完了？ → pop（g == 0 就代表這一輪失敗）
 *        - 這一個 face 的 3 個 turn 都試完了，或 face 是 last_face？ → 換下一個 face
 *        - 否則 → 產生下一個子節點，寫 path[g]，檢查剪枝和已解
 *                 通過就 push，++expanded
 */
static int my_search(uint16_t p, uint16_t o, uint8_t bound, uint8_t path[])
{
    frame_t *fr = stack;   /* 目前這一層，等於 &stack[g] */
    uint8_t g = 0;

    /* 根節點：呼叫前 my_solve 已確認不是已解，而且 h <= bound */
    fr->p = fr->np = p;
    fr->o = fr->no = o;
    fr->face = fr->turn = 0;
    fr->last_face = 3;
    ++expanded;

    for (;;) {
        if ( fr->face == 3) {
            if (g == 0)
                return -1;              /* 這一輪 bound 失敗 */
            /* ? pop：回到上一層 */
            --g;
            fr = &stack[g];
            continue;
        }
        if (fr->turn == 3 || fr->face == fr->last_face) {
            /* ? 換下一個 face：face + 1，turn 歸零，np/no 重設 */
            ++fr->face;
            fr->turn = 0;
            fr->np = fr->p;
            fr->no = fr->o;
            continue;
        }

        /* 產生下一個子節點 */
        fr->np = my_perm_move[fr->face][fr->np];
        fr->no = my_ori_move[fr->face][fr->no];
        path[g] = (fr->face << 1U) + fr->face + fr->turn;
        ++fr->turn;
        if (fr->np == 0 && fr->no == 0)
            return g + 1;
        if (g + 1 + my_h(fr->np, fr->no) > bound)
            continue;
        /* ? push：設定 fr + 1 的 7 個欄位，fr 往下一層，g + 1 */
        ++g;
        fr = &stack[g];
        fr->face = 0;
        fr->turn = 0;
        fr->last_face = stack[g - 1].face;
        fr->p = fr->np = stack[g - 1].np;
        fr->o = fr->no = stack[g - 1].no;
        ++expanded;
    }
}

/* 和第 4 題的 my_solve 一樣，只是呼叫 my_search。根節點已解要先處理。 */
static int my_solve(uint16_t p, uint16_t o, uint8_t path[])
{
    if (p == 0 && o == 0)
        return 0;

    uint8_t bound = my_h(p, o);

    while (bound <= 11) {
        int len = my_search(p, o, bound, path);
        if (len >= 0)
            return len;
        ++bound;
    }   

    return -1;
}

/* ---- 以下是測試，不用改 ---- */
#include <stdlib.h>
#include <time.h>

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
    if (argc == 2 && !strcmp(argv[1], "all")) {
        /* H3: every state, against the exact BFS distance. */
        state_t s;
        unsigned bad = 0;
        uint64_t max = 0;
        uint32_t worst = 0;
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (uint32_t r = 0; r < STATES; ++r) {
            char str[15];
            unrank_state(r, &s);
            for (int i = 0; i < CUBIES; ++i) {
                str[i] = (char) ('1' + s.p[i]);
                str[i + CUBIES] = (char) ('1' + s.o[i]);
            }
            str[14] = '\0';
            if (!check(str, &nodes, 0) && ++bad >= 5)
                break;
            if (nodes > max) {
                max = nodes;
                worst = r;
            }
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        printf("H3: %u states, %u failed, max expanded %llu (rank %u), "
               "%.1f s\n",
               STATES, bad, (unsigned long long) max, worst,
               (double) (t1.tv_sec - t0.tv_sec) +
                   (double) (t1.tv_nsec - t0.tv_nsec) * 1e-9);
        return bad != 0;
    }
    puts("usage: ex5 one STATE | chain | d11 | all");
    return 2;
}
