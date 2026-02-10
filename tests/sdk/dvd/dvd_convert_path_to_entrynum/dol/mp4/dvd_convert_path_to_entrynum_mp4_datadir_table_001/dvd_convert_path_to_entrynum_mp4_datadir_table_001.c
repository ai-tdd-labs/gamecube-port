#include <stdint.h>
#include <string.h>

#include <ogc/cache.h>

typedef uint32_t u32;
typedef int32_t s32;

#include "mp4_datadir_paths.h"

#ifndef GC_HOST_TEST
static s32 DVDConvertPathToEntrynum(char *pathPtr) {
    if (!pathPtr) return -1;
    for (u32 i = 0; i < (u32)k_mp4_datadir_paths_count; i++) {
        if (strcmp(k_mp4_datadir_paths[i], pathPtr) == 0) return (s32)i;
    }
    return -1;
}
#else
s32 DVDConvertPathToEntrynum(char *pathPtr);
#endif

static volatile u32 *OUT = (u32 *)0x80300000u;

int main(void) {
    u32 fails = 0;
    u32 checksum = 2166136261u;
    s32 r_first = -1;
    s32 r_last = -1;
    for (u32 i = 0; i < 16; i++) OUT[i] = 0;
    OUT[0] = 0xDEADBEEFu;
    OUT[1] = (u32)k_mp4_datadir_paths_count;

    for (u32 i = 0; i < (u32)k_mp4_datadir_paths_count; i++) {
        s32 r = DVDConvertPathToEntrynum((char *)k_mp4_datadir_paths[i]);
        if (r == -1) fails++;
        if (i == 0) r_first = r;
        if (i + 1 == (u32)k_mp4_datadir_paths_count) r_last = r;
        checksum ^= (u32)r + 0x9e3779b9u + i;
        checksum *= 16777619u;
    }

    OUT[2] = fails;
    OUT[3] = (u32)DVDConvertPathToEntrynum("data/does_not_exist.bin");
    OUT[4] = (u32)r_first;
    OUT[5] = (u32)r_last;
    OUT[6] = checksum;
    OUT[7] = checksum ^ 0xA5A5A5A5u;

    // Make the debugger's RAM reads deterministic.
    DCFlushRange((void *)0x80300000u, 0x40);

    for (;;) {}
    return 0;
}
