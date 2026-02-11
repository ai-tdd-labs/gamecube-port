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

typedef void (*DVDCallback)(s32 result, DVDFileInfo *fileInfo);

void gc_dvd_test_reset_files(void);
void gc_dvd_test_set_file(s32 entrynum, const void *data, u32 len);
int DVDFastOpen(s32 entrynum, DVDFileInfo *file);
s32 DVDReadAsyncPrio(DVDFileInfo *file, void *addr, s32 len, s32 offset, DVDCallback cb, s32 prio);

static uint32_t xs32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static s32 g_cb_calls;
static s32 g_cb_last;
static void test_cb(s32 result, DVDFileInfo *fileInfo) {
    (void)fileInfo;
    g_cb_calls++;
    g_cb_last = result;
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
        int use_cb = (xs32(&seed) & 1u) != 0;
        uint8_t dst[8192];
        memset(dst, 0xCC, sizeof(dst));
        g_cb_calls = 0;
        g_cb_last = 0;

        s32 got = DVDReadAsyncPrio(&fi, dst, len, off, use_cb ? test_cb : NULL, prio);
        strict_dvd_read_window_t strict = strict_dvd_read_window(file_len, off, len);
        if (got != strict.ok) {
            fprintf(stderr, "PBT FAIL: async_prio ret got=%d exp=%d\n", got, strict.ok);
            free(src);
            return 1;
        }
        if (use_cb) {
            if (g_cb_calls != 1) {
                fprintf(stderr, "PBT FAIL: callback calls=%d exp=1\n", g_cb_calls);
                free(src);
                return 1;
            }
            if (g_cb_last != strict.sync_ret) {
                fprintf(stderr, "PBT FAIL: callback result got=%d exp=%d\n", g_cb_last, strict.sync_ret);
                free(src);
                return 1;
            }
        } else if (g_cb_calls != 0) {
            fprintf(stderr, "PBT FAIL: callback unexpectedly called\n");
            free(src);
            return 1;
        }

        if (strict.n >= 0 && memcmp(dst, src + off, (size_t)strict.n) != 0) {
            fprintf(stderr, "PBT FAIL: copied bytes mismatch\n");
            free(src);
            return 1;
        }
        free(src);
    }

    printf("PBT PASS: dvd_read_async_prio %u iterations\n", iters);
    return 0;
}
