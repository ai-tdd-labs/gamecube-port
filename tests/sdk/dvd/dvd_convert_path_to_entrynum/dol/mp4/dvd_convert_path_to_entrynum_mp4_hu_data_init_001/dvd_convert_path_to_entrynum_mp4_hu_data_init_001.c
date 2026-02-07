#include <stdint.h>
#include <string.h>

typedef uint32_t u32;
typedef int32_t s32;

// MP4 callsite reference:
//   decomp_mario_party_4/src/game/data.c: HuDataInit()
// It loops over "data/*.bin" paths and panics if DVDConvertPathToEntrynum() returns -1.
static const char *k_mp4_data_paths[] = {
    "data/E3setup.bin",
    "data/bbattle.bin",
    "data/bguest.bin",
    "data/bkoopa.bin",
    "data/bkoopasuit.bin",
};

#ifndef GC_HOST_TEST
// DOL oracle stub (deterministic): return index for known paths, else -1.
static s32 DVDConvertPathToEntrynum(char *pathPtr) {
    if (!pathPtr) return -1;
    for (u32 i = 0; i < (u32)(sizeof(k_mp4_data_paths) / sizeof(k_mp4_data_paths[0])); i++) {
        if (strcmp(k_mp4_data_paths[i], pathPtr) == 0) return (s32)i;
    }
    return -1;
}
#else
// Host uses the real sdk_port implementation.
s32 DVDConvertPathToEntrynum(char *pathPtr);
#endif

static volatile u32 *OUT = (u32 *)0x80300000u;

int main(void) {
    OUT[0] = 0xDEADBEEFu; // marker: test ran
    OUT[1] = (u32)(sizeof(k_mp4_data_paths) / sizeof(k_mp4_data_paths[0]));

    for (u32 i = 0; i < OUT[1]; i++) {
        s32 r = DVDConvertPathToEntrynum((char *)k_mp4_data_paths[i]);
        OUT[2 + i] = (u32)r;
    }

    // Also check a path that is not in the table (HuDataInit would panic on this).
    OUT[2 + OUT[1]] = (u32)DVDConvertPathToEntrynum("data/does_not_exist.bin");

    for (;;) {}
    return 0;
}

