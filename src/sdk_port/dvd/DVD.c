#include <stdint.h>
#include <stdlib.h>
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
//
// We do not emulate the real drive/FST. Instead, tests inject a tiny "virtual
// disc" mapping from entrynum -> bytes so MP4 callsites can be exercised
// deterministically (e.g. HuDvdDataFastRead uses DVDFastOpen+DVDReadAsync).

typedef struct {
    volatile s32 state; // 0 = idle, 1 = busy
} DVDCommandBlock;

typedef struct {
    DVDCommandBlock cb;
    u32 startAddr; // token only; used by MP4 async callback matching
    u32 length;
    s32 entrynum;
} DVDFileInfo;

typedef void (*DVDCallback)(s32 result, DVDFileInfo* fileInfo);

typedef struct {
    uint8_t *data;
    u32 len;
} GcDvdTestFile;

static GcDvdTestFile g_dvd_test_files[512];

// Forward decl (defined later in this file).
s32 DVDConvertPathToEntrynum(char *pathPtr);

static void gc_dvd_test_reset_files_internal(void) {
    for (u32 i = 0; i < (u32)(sizeof(g_dvd_test_files) / sizeof(g_dvd_test_files[0])); i++) {
        free(g_dvd_test_files[i].data);
        g_dvd_test_files[i].data = NULL;
        g_dvd_test_files[i].len = 0;
    }
}

void gc_dvd_test_reset_files(void) {
    gc_dvd_test_reset_files_internal();
}

void gc_dvd_test_set_file(s32 entrynum, const void *data, u32 len) {
    if (entrynum < 0) return;
    if ((u32)entrynum >= (u32)(sizeof(g_dvd_test_files) / sizeof(g_dvd_test_files[0]))) return;

    free(g_dvd_test_files[entrynum].data);
    g_dvd_test_files[entrynum].data = NULL;
    g_dvd_test_files[entrynum].len = 0;

    if (!data || len == 0) return;
    uint8_t *buf = (uint8_t *)malloc(len);
    if (!buf) return;
    memcpy(buf, data, len);
    g_dvd_test_files[entrynum].data = buf;
    g_dvd_test_files[entrynum].len = len;
}

int DVDGetCommandBlockStatus(DVDCommandBlock *block) {
    if (!block) return 0;
    return block->state;
}

int DVDFastOpen(s32 entrynum, DVDFileInfo *file) {
    if (!file) return 0;
    if (entrynum < 0) return 0;
    if ((u32)entrynum >= (u32)(sizeof(g_dvd_test_files) / sizeof(g_dvd_test_files[0]))) return 0;
    if (!g_dvd_test_files[entrynum].data) return 0;

    file->cb.state = 0;
    file->entrynum = entrynum;
    file->startAddr = (u32)entrynum; // stable token; not a real disc address
    file->length = g_dvd_test_files[entrynum].len;
    return 1;
}

int DVDOpen(const char *path, DVDFileInfo *file) {
    (void)path;
    // Deterministic test backend: use path->entrynum mapping when present.
    s32 entry = DVDConvertPathToEntrynum((char *)path);

    gc_dvd_open_calls = gc_sdk_state_load_u32_or(0x350, gc_dvd_open_calls) + 1u;
    gc_sdk_state_store_u32be(0x350, gc_dvd_open_calls);
    return DVDFastOpen(entry, file);
}

int DVDRead(DVDFileInfo *file, void *addr, int len, int offset) {
    if (!file || !addr) return -1;
    gc_dvd_read_calls = gc_sdk_state_load_u32_or(0x354, gc_dvd_read_calls) + 1u;
    gc_sdk_state_store_u32be(0x354, gc_dvd_read_calls);
    gc_dvd_last_read_len = (u32)len;
    gc_dvd_last_read_off = (u32)offset;
    gc_sdk_state_store_u32be(0x358, gc_dvd_last_read_len);
    gc_sdk_state_store_u32be(0x35C, gc_dvd_last_read_off);

    // Synchronous read is implemented on top of our test file table.
    s32 entry = file->entrynum;
    if (entry < 0) return -1;
    if ((u32)entry >= (u32)(sizeof(g_dvd_test_files) / sizeof(g_dvd_test_files[0]))) return -1;
    if (!g_dvd_test_files[entry].data) return -1;
    if (offset < 0 || len < 0) return -1;

    u32 off = (u32)offset;
    u32 n = (u32)len;
    if (off > g_dvd_test_files[entry].len) return -1;
    if (off + n > g_dvd_test_files[entry].len) {
        n = g_dvd_test_files[entry].len - off;
    }
    memcpy(addr, g_dvd_test_files[entry].data + off, n);
    return (int)n;
}

// SDK signature: s32 DVDReadAsync(DVDFileInfo*, void*, s32, s32, DVDCallback)
s32 DVDReadAsync(DVDFileInfo *file, void *addr, s32 len, s32 offset, DVDCallback cb) {
    if (!file || !addr) return 0;

    // Mark "busy", do the copy immediately, then mark idle.
    file->cb.state = 1;
    int n = DVDRead(file, addr, (int)len, (int)offset);
    file->cb.state = 0;

    if (cb) {
        cb((n < 0) ? (s32)-1 : (s32)n, file);
    }
    return (n < 0) ? 0 : 1;
}

s32 DVDReadPrio(DVDFileInfo *file, void *addr, s32 len, s32 offset, s32 prio) {
    (void)prio;
    return (DVDRead(file, addr, (int)len, (int)offset) < 0) ? -1 : 0;
}

s32 DVDReadAsyncPrio(DVDFileInfo *file, void *addr, s32 len, s32 offset, DVDCallback cb, s32 prio) {
    (void)prio;
    return DVDReadAsync(file, addr, len, offset, cb);
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
