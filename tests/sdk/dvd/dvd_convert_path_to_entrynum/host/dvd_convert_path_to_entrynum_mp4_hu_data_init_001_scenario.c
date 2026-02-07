#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef int32_t s32;

// SDK function under test (implemented in src/sdk_port/dvd/DVD.c)
s32 DVDConvertPathToEntrynum(char *pathPtr);

// Test-only injection hook (implemented in src/sdk_port/dvd/DVD.c)
void gc_dvd_test_set_paths(const char **paths, s32 count);

static const char *k_mp4_data_paths[] = {
    "data/E3setup.bin",
    "data/bbattle.bin",
    "data/bguest.bin",
    "data/bkoopa.bin",
    "data/bkoopasuit.bin",
};

const char *gc_scenario_label(void) { return "DVDConvertPathToEntrynum/mp4_hu_data_init"; }
const char *gc_scenario_out_path(void) { return "../actual/dvd_convert_path_to_entrynum_mp4_hu_data_init_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    gc_dvd_test_set_paths(k_mp4_data_paths, (s32)(sizeof(k_mp4_data_paths) / sizeof(k_mp4_data_paths[0])));

    const uint32_t base = 0x80300000u;
    const uint32_t n = (uint32_t)(sizeof(k_mp4_data_paths) / sizeof(k_mp4_data_paths[0]));

    uint8_t *p = gc_ram_ptr(ram, base, 8 + 4 * (n + 1));
    if (!p) die("gc_ram_ptr failed");

    wr32be(p + 0, 0xDEADBEEFu);
    wr32be(p + 4, n);

    for (uint32_t i = 0; i < n; i++) {
        s32 r = DVDConvertPathToEntrynum((char *)k_mp4_data_paths[i]);
        wr32be(p + 8 + 4 * i, (uint32_t)r);
    }

    // Also check a missing path.
    s32 miss = DVDConvertPathToEntrynum("data/does_not_exist.bin");
    wr32be(p + 8 + 4 * n, (uint32_t)miss);
}

