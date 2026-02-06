#include <stdint.h>

typedef uint32_t u32;

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
