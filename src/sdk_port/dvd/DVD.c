#include <stdint.h>
#include <string.h>

typedef uint32_t u32;
typedef int32_t s32;

// RAM-backed state (big-endian in MEM1) for dump comparability.
#include "../sdk_state.h"

u32 gc_dvd_initialized;
u32 gc_dvd_drive_status;
u32 gc_dvd_open_calls;
u32 gc_dvd_read_calls;
u32 gc_dvd_close_calls;
u32 gc_dvd_last_read_len;
u32 gc_dvd_last_read_off;

void DVDInit(void) {
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_DVD_INITIALIZED, &gc_dvd_initialized, 1u);
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_DVD_DRIVE_STATUS, &gc_dvd_drive_status, 0x12345678u);
}

u32 DVDGetDriveStatus(void) {
    gc_dvd_drive_status = gc_sdk_state_load_u32_or(GC_SDK_OFF_DVD_DRIVE_STATUS, gc_dvd_drive_status);
    return gc_dvd_drive_status;
}

// Minimal DVD API surface for deterministic tests.
// We don't model the drive; we just record calls and return stable values.

typedef struct {
    u32 _dummy;
} DVDFileInfo;

int DVDOpen(DVDFileInfo *file, const char *path) {
    (void)file;
    (void)path;
    gc_dvd_open_calls = gc_sdk_state_load_u32_or(0x350, gc_dvd_open_calls) + 1u;
    gc_sdk_state_store_u32be(0x350, gc_dvd_open_calls);
    return 1;
}

int DVDRead(DVDFileInfo *file, void *addr, int len, int offset) {
    (void)file;
    (void)addr;
    gc_dvd_read_calls = gc_sdk_state_load_u32_or(0x354, gc_dvd_read_calls) + 1u;
    gc_sdk_state_store_u32be(0x354, gc_dvd_read_calls);
    gc_dvd_last_read_len = (u32)len;
    gc_dvd_last_read_off = (u32)offset;
    gc_sdk_state_store_u32be(0x358, gc_dvd_last_read_len);
    gc_sdk_state_store_u32be(0x35C, gc_dvd_last_read_off);
    return len;
}

int DVDClose(DVDFileInfo *file) {
    (void)file;
    gc_dvd_close_calls = gc_sdk_state_load_u32_or(0x360, gc_dvd_close_calls) + 1u;
    gc_sdk_state_store_u32be(0x360, gc_dvd_close_calls);
    return 1;
}

// -----------------------------------------------------------------------------
// DVD filesystem path -> entrynum
//
// In the real SDK this walks the disc FST. We do not have a full disc model yet.
//
// For now we provide a tiny injectable map so we can write deterministic tests
// for MP4 callsites (e.g. HuDataInit validates that its data/*.bin paths exist).
// -----------------------------------------------------------------------------

static const char *g_dvd_test_paths[512];
static s32 g_dvd_test_path_count;

void gc_dvd_test_reset_paths(void) {
    for (u32 i = 0; i < (u32)g_dvd_test_path_count; i++) {
        g_dvd_test_paths[i] = 0;
    }
    g_dvd_test_path_count = 0;
}

void gc_dvd_test_set_paths(const char **paths, s32 count) {
    gc_dvd_test_reset_paths();
    if (!paths || count <= 0) {
        return;
    }
    if (count > (s32)(sizeof(g_dvd_test_paths) / sizeof(g_dvd_test_paths[0]))) {
        count = (s32)(sizeof(g_dvd_test_paths) / sizeof(g_dvd_test_paths[0]));
    }
    for (s32 i = 0; i < count; i++) {
        g_dvd_test_paths[i] = paths[i];
    }
    g_dvd_test_path_count = count;
}

// SDK signature (from decomp): s32 DVDConvertPathToEntrynum(char* pathPtr)
s32 DVDConvertPathToEntrynum(char *pathPtr) {
    if (!pathPtr) {
        return -1;
    }
    // Deterministic test backend: return the index for known paths.
    for (s32 i = 0; i < g_dvd_test_path_count; i++) {
        const char *p = g_dvd_test_paths[i];
        if (!p) {
            continue;
        }
        if (strcmp(p, pathPtr) == 0) {
            return i;
        }
    }
    return -1;
}
