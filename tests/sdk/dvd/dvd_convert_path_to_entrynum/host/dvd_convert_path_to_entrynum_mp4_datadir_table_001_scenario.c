#include <stdint.h>
#include <string.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef int32_t s32;

s32 DVDConvertPathToEntrynum(char *pathPtr);
void gc_dvd_test_set_paths(const char **paths, s32 count);

// Reuse the same path list as the DOL.
#include "../dol/mp4/dvd_convert_path_to_entrynum_mp4_datadir_table_001/mp4_datadir_paths.h"

const char *gc_scenario_label(void) { return "DVDConvertPathToEntrynum/mp4_datadir_table"; }
const char *gc_scenario_out_path(void) { return "../actual/dvd_convert_path_to_entrynum_mp4_datadir_table_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    gc_dvd_test_set_paths(k_mp4_datadir_paths, (s32)k_mp4_datadir_paths_count);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x40);
    if (!p) die("gc_ram_ptr failed");
    memset(p, 0, 0x40);

    uint32_t fails = 0;
    uint32_t checksum = 2166136261u;
    s32 r_first = -1;
    s32 r_last = -1;
    for (uint32_t i = 0; i < (uint32_t)k_mp4_datadir_paths_count; i++) {
        s32 r = DVDConvertPathToEntrynum((char *)k_mp4_datadir_paths[i]);
        if (r == -1) fails++;
        if (i == 0) r_first = r;
        if (i + 1 == (uint32_t)k_mp4_datadir_paths_count) r_last = r;
        checksum ^= (uint32_t)r + 0x9e3779b9u + i;
        checksum *= 16777619u;
    }

    wr32be(p + 0, 0xDEADBEEFu);
    wr32be(p + 4, (uint32_t)k_mp4_datadir_paths_count);
    wr32be(p + 8, fails);
    wr32be(p + 12, (uint32_t)DVDConvertPathToEntrynum("data/does_not_exist.bin"));
    wr32be(p + 16, (uint32_t)r_first);
    wr32be(p + 20, (uint32_t)r_last);
    wr32be(p + 24, checksum);
    wr32be(p + 28, checksum ^ 0xA5A5A5A5u);
}
