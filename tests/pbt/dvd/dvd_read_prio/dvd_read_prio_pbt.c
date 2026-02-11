#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../dvd_core_strict_oracle.h"
#include "../../../../src/sdk_port/gc_mem.h"
#include "../../../../src/sdk_port/sdk_state.h"

typedef int32_t s32;
typedef uint32_t u32;

typedef struct {
    volatile s32 state;
} DVDCommandBlock;

typedef struct {
    DVDCommandBlock cb;
    u32 startAddr;
    u32 length;
    s32 entrynum;
} DVDFileInfo;

void gc_dvd_test_reset_files(void);
void gc_dvd_test_set_file(s32 entrynum, const void *data, u32 len);
int DVDFastOpen(s32 entrynum, DVDFileInfo *file);
s32 DVDReadPrio(DVDFileInfo *file, void *addr, s32 len, s32 offset, s32 prio);
int DVDRead(DVDFileInfo *file, void *addr, int len, int offset);

static uint32_t xs32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

int main(int argc, char **argv) {
    uint32_t iters = 200000;
    uint32_t seed = 0xC0DEC0DEu;
    if (argc >= 2) iters = (uint32_t)strtoul(argv[1], 0, 0);
    if (argc >= 3) seed = (uint32_t)strtoul(argv[2], 0, 0);

    static uint8_t mem1[0x01800000];
    gc_mem_set(0x80000000u, sizeof(mem1), mem1);
    gc_sdk_state_reset();

    for (uint32_t i = 0; i < iters; i++) {
        gc_dvd_test_reset_files();
        uint32_t file_len = (xs32(&seed) % 4096u) + 1u;
        uint8_t *src = (uint8_t *)malloc(file_len);
        if (!src) return 2;
        for (uint32_t j = 0; j < file_len; j++) src[j] = (uint8_t)(xs32(&seed) & 0xFFu);
        gc_dvd_test_set_file(0, src, file_len);

        DVDFileInfo fi;
        memset(&fi, 0, sizeof(fi));
        if (!DVDFastOpen(0, &fi)) {
            fprintf(stderr, "PBT FAIL: DVDFastOpen failed\n");
            free(src);
            return 1;
        }

        s32 off = (s32)(xs32(&seed) % 5000u) - 500;
        s32 len = (s32)(xs32(&seed) % 5000u) - 500;
        s32 prio = (s32)(xs32(&seed) & 0x7FFFFFFFu);

        uint8_t dst0[8192], dst1[8192];
        memset(dst0, 0xAA, sizeof(dst0));
        memset(dst1, 0xBB, sizeof(dst1));

        s32 got_prio = DVDReadPrio(&fi, dst0, len, off, prio);
        int got_sync = DVDRead(&fi, dst1, (int)len, (int)off);

        strict_dvd_read_window_t strict = strict_dvd_read_window(file_len, off, len);
        s32 exp_prio = strict.prio_ret;
        if (got_prio != exp_prio) {
            fprintf(stderr, "PBT FAIL: DVDReadPrio ret got=%d exp=%d\n", got_prio, exp_prio);
            free(src);
            return 1;
        }
        if ((got_sync < 0) != (strict.sync_ret < 0)) {
            fprintf(stderr, "PBT FAIL: sync/prio error parity mismatch\n");
            free(src);
            return 1;
        }

        if (strict.n >= 0) {
            if (memcmp(dst0, src + off, (size_t)strict.n) != 0 ||
                memcmp(dst1, src + off, (size_t)strict.n) != 0) {
                fprintf(stderr, "PBT FAIL: copied bytes mismatch\n");
                free(src);
                return 1;
            }
        }
        free(src);
    }

    printf("PBT PASS: dvd_read_prio %u iterations\n", iters);
    return 0;
}
