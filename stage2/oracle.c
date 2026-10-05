/* oracle -- exact distance table and the distance-11 test set (host only)
 *
 * Writes:
 *   out/dist.bin   3,674,160 bytes, dist[rank]
 *   out/d11.txt    one "rank state" line per distance-11 state (2,644 lines)
 * and prints the depth distribution for comparison with report.md.
 */
#include "oracle_lib.h"

int main(void)
{
    double t0 = seconds_now();
    uint8_t *dist = oracle_distances();
    if (!dist) {
        fputs("oracle: could not build the distance table\n", stderr);
        return 1;
    }
    uint32_t hist[12] = {0};
    for (uint32_t rank = 0; rank < STATES; ++rank) {
        if (dist[rank] > 11) {
            fprintf(stderr, "oracle: rank %u has distance %u\n", rank,
                    dist[rank]);
            return 1;
        }
        ++hist[dist[rank]];
    }
    printf("depth  states\n");
    for (int d = 0; d <= 11; ++d)
        printf("%5d  %7u\n", d, hist[d]);

    FILE *f = fopen("out/dist.bin", "wb");
    if (!f || fwrite(dist, 1, STATES, f) != STATES || fclose(f)) {
        fputs("oracle: cannot write out/dist.bin\n", stderr);
        return 1;
    }
    f = fopen("out/d11.txt", "w");
    if (!f) {
        fputs("oracle: cannot write out/d11.txt\n", stderr);
        return 1;
    }
    char str[15];
    int sample_found = 0;
    for (uint32_t rank = 0; rank < STATES; ++rank) {
        if (dist[rank] != 11)
            continue;
        rank_to_string(rank, str);
        fprintf(f, "%u %s\n", rank, str);
        sample_found |= !strcmp(str, "21345671111111");
    }
    if (fclose(f)) {
        fputs("oracle: cannot write out/d11.txt\n", stderr);
        return 1;
    }
    printf("distance-11 states: %u (sample 21345671111111 %s)\n", hist[11],
           sample_found ? "included" : "MISSING");
    printf("time: %.2f s\n", seconds_now() - t0);
    free(dist);
    return hist[11] == 2644 && sample_found ? 0 : 1;
}
