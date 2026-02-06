#include <stdint.h>

typedef uint32_t u32;

u32 gc_dvd_initialized;
u32 gc_dvd_drive_status;
u32 gc_dvd_open_calls;
u32 gc_dvd_read_calls;
u32 gc_dvd_close_calls;
u32 gc_dvd_last_read_len;
u32 gc_dvd_last_read_off;

void DVDInit(void) {
    gc_dvd_initialized = 1;
    gc_dvd_drive_status = 0x12345678u;
}

u32 DVDGetDriveStatus(void) {
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
    gc_dvd_open_calls++;
    return 1;
}

int DVDRead(DVDFileInfo *file, void *addr, int len, int offset) {
    (void)file;
    (void)addr;
    gc_dvd_read_calls++;
    gc_dvd_last_read_len = (u32)len;
    gc_dvd_last_read_off = (u32)offset;
    return len;
}

int DVDClose(DVDFileInfo *file) {
    (void)file;
    gc_dvd_close_calls++;
    return 1;
}
