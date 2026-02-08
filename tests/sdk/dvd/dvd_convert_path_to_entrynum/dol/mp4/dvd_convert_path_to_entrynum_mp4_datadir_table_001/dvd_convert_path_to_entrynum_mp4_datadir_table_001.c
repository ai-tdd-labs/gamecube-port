#include <stdint.h>
#include <string.h>

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
    OUT[0] = 0xDEADBEEFu;
    OUT[1] = (u32)k_mp4_datadir_paths_count;

    for (u32 i = 0; i < (u32)k_mp4_datadir_paths_count; i++) {
        s32 r = DVDConvertPathToEntrynum((char *)k_mp4_datadir_paths[i]);
        if (r == -1) fails++;
    }

    OUT[2] = fails;
    OUT[3] = (u32)DVDConvertPathToEntrynum("data/does_not_exist.bin");

    for (;;) {}
    return 0;
}
