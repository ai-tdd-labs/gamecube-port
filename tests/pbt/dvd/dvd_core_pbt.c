#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../src/sdk_port/gc_mem.h"
#include "../../../src/sdk_port/sdk_state.h"

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
void gc_dvd_test_reset_paths(void);
void gc_dvd_test_set_paths(const char **paths, s32 count);
int DVDFastOpen(s32 entrynum, DVDFileInfo *file);
int DVDOpen(const char *path, DVDFileInfo *file);
s32 DVDReadPrio(DVDFileInfo *file, void *addr, s32 len, s32 offset, s32 prio);
s32 DVDReadAsyncPrio(DVDFileInfo *file, void *addr, s32 len, s32 offset, DVDCallback cb, s32 prio);
int DVDRead(DVDFileInfo *file, void *addr, int len, int offset);
int DVDClose(DVDFileInfo *file);
s32 DVDConvertPathToEntrynum(char *pathPtr);
s32 DVDReadAsync(DVDFileInfo *file, void *addr, s32 len, s32 offset, DVDCallback cb);
int DVDGetCommandBlockStatus(DVDCommandBlock *block);

static uint32_t xs32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static int g_cb_calls;
static s32 g_cb_last_result;

static void test_cb(s32 result, DVDFileInfo *fileInfo) {
    (void)fileInfo;
    g_cb_calls++;
    g_cb_last_result = result;
}

int main(int argc, char **argv) {
    uint32_t iters = 200000;
    uint32_t seed = 0xC0DEC0DEu;
    if (argc >= 2) iters = (uint32_t)strtoul(argv[1], 0, 0);
    if (argc >= 3) seed = (uint32_t)strtoul(argv[2], 0, 0);

    // Minimal MEM1 map for sdk_state usage.
    static uint8_t mem1[0x01800000];
    gc_mem_set(0x80000000u, sizeof(mem1), mem1);
    gc_sdk_state_reset();

    for (uint32_t i = 0; i < iters; i++) {
        gc_dvd_test_reset_files();
        gc_dvd_test_reset_paths();

        uint32_t file_len = xs32(&seed) % 4096u;
        if (file_len == 0) file_len = 1;
        uint8_t *src = (uint8_t *)malloc(file_len);
        if (!src) return 2;
        for (uint32_t j = 0; j < file_len; j++) {
            src[j] = (uint8_t)(xs32(&seed) & 0xFFu);
        }
        gc_dvd_test_set_file(0, src, file_len);
        const char *paths[1] = {"data/test.bin"};
        gc_dvd_test_set_paths(paths, 1);
        if (DVDConvertPathToEntrynum("data/test.bin") != 0 || DVDConvertPathToEntrynum("data/missing.bin") != -1) {
            fprintf(stderr, "PBT FAIL: DVDConvertPathToEntrynum mapping mismatch\n");
            free(src);
            return 1;
        }

        DVDFileInfo fi;
        memset(&fi, 0, sizeof(fi));
        if (!DVDFastOpen(0, &fi)) {
            fprintf(stderr, "PBT FAIL: DVDFastOpen expected success\n");
            free(src);
            return 1;
        }
        DVDFileInfo fi_open;
        memset(&fi_open, 0, sizeof(fi_open));
        if (!DVDOpen("data/test.bin", &fi_open)) {
            fprintf(stderr, "PBT FAIL: DVDOpen expected success for mapped path\n");
            free(src);
            return 1;
        }
        if (fi.length != file_len) {
            fprintf(stderr, "PBT FAIL: DVDFastOpen length mismatch got=%u exp=%u\n", fi.length, file_len);
            free(src);
            return 1;
        }

        int32_t off = (int32_t)(xs32(&seed) % 5000u) - 500;
        int32_t len = (int32_t)(xs32(&seed) % 5000u) - 500;
        uint8_t dst[8192];
        memset(dst, 0xCD, sizeof(dst));
        g_cb_calls = 0;
        g_cb_last_result = 0;

        int use_cb = (xs32(&seed) & 1u) != 0;
        int cb_before_async = g_cb_calls;
        s32 ok = DVDReadAsync(&fi, dst, len, off, use_cb ? test_cb : NULL);

        s32 exp_n = -1;
        if (off >= 0 && len >= 0 && (u32)off <= file_len) {
            u32 avail = file_len - (u32)off;
            u32 want = (u32)len;
            exp_n = (s32)((want <= avail) ? want : avail);
        }

        s32 exp_ok = (exp_n < 0) ? 0 : 1;
        if (ok != exp_ok) {
            fprintf(stderr, "PBT FAIL: DVDReadAsync return mismatch ok=%d exp=%d off=%d len=%d file_len=%u\n",
                    ok, exp_ok, off, len, file_len);
            free(src);
            return 1;
        }
        if (use_cb) {
            if ((g_cb_calls - cb_before_async) != 1) {
                fprintf(stderr, "PBT FAIL: callback count after DVDReadAsync delta=%d\n", g_cb_calls - cb_before_async);
                free(src);
                return 1;
            }
            s32 exp_cb = (exp_n < 0) ? -1 : exp_n;
            if (g_cb_last_result != exp_cb) {
                fprintf(stderr, "PBT FAIL: callback result got=%d exp=%d\n", g_cb_last_result, exp_cb);
                free(src);
                return 1;
            }
        } else if ((g_cb_calls - cb_before_async) != 0) {
            fprintf(stderr, "PBT FAIL: callback unexpectedly called in DVDReadAsync\n");
            free(src);
            return 1;
        }

        if (DVDGetCommandBlockStatus(&fi.cb) != 0) {
            fprintf(stderr, "PBT FAIL: command block not idle\n");
            free(src);
            return 1;
        }

        if (exp_n >= 0) {
            if (memcmp(dst, src + off, (size_t)exp_n) != 0) {
                fprintf(stderr, "PBT FAIL: copied bytes mismatch\n");
                free(src);
                return 1;
            }
        }

        // Synchronous path parity with same parameters.
        uint8_t dst_sync[8192];
        memset(dst_sync, 0xEE, sizeof(dst_sync));
        int n_sync = DVDRead(&fi_open, dst_sync, len, off);
        int exp_sync = (exp_n < 0) ? -1 : exp_n;
        if (n_sync != exp_sync) {
            fprintf(stderr, "PBT FAIL: DVDRead return mismatch got=%d exp=%d\n", n_sync, exp_sync);
            free(src);
            return 1;
        }
        if (exp_n >= 0 && memcmp(dst_sync, src + off, (size_t)exp_n) != 0) {
            fprintf(stderr, "PBT FAIL: DVDRead copied bytes mismatch\n");
            free(src);
            return 1;
        }

        // Prio wrappers should preserve result semantics.
        int n_prio = DVDReadPrio(&fi_open, dst_sync, len, off, 2);
        int exp_prio = (exp_n < 0) ? -1 : 0;
        if (n_prio != exp_prio) {
            fprintf(stderr, "PBT FAIL: DVDReadPrio return mismatch got=%d exp=%d\n", n_prio, exp_prio);
            free(src);
            return 1;
        }
        int cb_before_async_prio = g_cb_calls;
        s32 ok_async_prio = DVDReadAsyncPrio(&fi_open, dst_sync, len, off, use_cb ? test_cb : NULL, 2);
        if (ok_async_prio != exp_ok) {
            fprintf(stderr, "PBT FAIL: DVDReadAsyncPrio return mismatch got=%d exp=%d\n", ok_async_prio, exp_ok);
            free(src);
            return 1;
        }

        if (use_cb) {
            if ((g_cb_calls - cb_before_async_prio) != 1) {
                fprintf(stderr, "PBT FAIL: callback count after DVDReadAsyncPrio delta=%d\n", g_cb_calls - cb_before_async_prio);
                free(src);
                return 1;
            }
        } else if ((g_cb_calls - cb_before_async_prio) != 0) {
            fprintf(stderr, "PBT FAIL: callback unexpectedly called in DVDReadAsyncPrio\n");
            free(src);
            return 1;
        }

        if (!DVDClose(&fi_open)) {
            fprintf(stderr, "PBT FAIL: DVDClose expected success\n");
            free(src);
            return 1;
        }

        free(src);
    }

    printf("PBT PASS: dvd_core %u iterations\n", iters);
    return 0;
}
