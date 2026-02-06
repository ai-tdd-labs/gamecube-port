#include <stdint.h>

typedef uint32_t u32;

u32 gc_dvd_initialized;
u32 gc_dvd_drive_status;

void DVDInit(void) {
    gc_dvd_initialized = 1;
    gc_dvd_drive_status = 0x12345678u;
}

u32 DVDGetDriveStatus(void) {
    return gc_dvd_drive_status;
}

