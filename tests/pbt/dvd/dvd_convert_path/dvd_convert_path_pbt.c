#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../../src/sdk_port/gc_mem.h"
#include "../../../../src/sdk_port/sdk_state.h"

typedef int32_t s32;

void gc_dvd_test_reset_paths(void);
void gc_dvd_test_set_paths(const char **paths, s32 count);
s32 DVDConvertPathToEntrynum(char *pathPtr);

static uint32_t xs32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static char rnd_char(uint32_t *s) {
    const char alnum[] = "abcdefghijklmnopqrstuvwxyz0123456789_./";
    return alnum[xs32(s) % (sizeof(alnum) - 1)];
}

int main(int argc, char **argv) {
    uint32_t iters = 200000;
    uint32_t seed = 0xC0DEC0DEu;
    if (argc >= 2) iters = (uint32_t)strtoul(argv[1], 0, 0);
    if (argc >= 3) seed = (uint32_t)strtoul(argv[2], 0, 0);

    static uint8_t mem1[0x01800000];
    gc_mem_set(0x80000000u, sizeof(mem1), mem1);
    gc_sdk_state_reset();

    // Force fallback map mode (no FST).
    gc_dvd_test_reset_paths();

    for (uint32_t i = 0; i < iters; i++) {
        enum { MAXP = 12, MAXL = 40 };
        static char paths[MAXP][MAXL];
        const char *path_ptrs[MAXP];
        s32 n = (s32)((xs32(&seed) % MAXP) + 1u);
        for (s32 p = 0; p < n; p++) {
            int unique = 0;
            while (!unique) {
                int len = (int)(xs32(&seed) % (MAXL - 2)) + 1;
                for (int k = 0; k < len; k++) paths[p][k] = rnd_char(&seed);
                paths[p][len] = '\0';
                unique = 1;
                for (s32 q = 0; q < p; q++) {
                    if (strcmp(paths[p], paths[q]) == 0) {
                        unique = 0;
                        break;
                    }
                }
            }
            path_ptrs[p] = paths[p];
        }
        gc_dvd_test_set_paths(path_ptrs, n);

        // All mapped paths must map to their index.
        for (s32 p = 0; p < n; p++) {
            s32 got = DVDConvertPathToEntrynum(paths[p]);
            if (got != p) {
                fprintf(stderr, "PBT FAIL: mapped path idx mismatch got=%d exp=%d path=%s\n", got, p, paths[p]);
                return 1;
            }
        }

        // One missing random path must return -1.
        char miss[MAXL];
        int mlen = (int)(xs32(&seed) % (MAXL - 2)) + 1;
        for (int k = 0; k < mlen; k++) miss[k] = rnd_char(&seed);
        miss[mlen] = '\0';
        int collides = 0;
        for (s32 p = 0; p < n; p++) if (strcmp(miss, paths[p]) == 0) collides = 1;
        if (collides) miss[0] = (miss[0] == 'x') ? 'y' : 'x';
        if (DVDConvertPathToEntrynum(miss) != -1) {
            fprintf(stderr, "PBT FAIL: missing path resolved unexpectedly: %s\n", miss);
            return 1;
        }

        if (DVDConvertPathToEntrynum(NULL) != -1) {
            fprintf(stderr, "PBT FAIL: NULL path should be -1\n");
            return 1;
        }
    }

    printf("PBT PASS: dvd_convert_path %u iterations\n", iters);
    return 0;
}
