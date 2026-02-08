#include <stdint.h>

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

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 16);
    if (!p) die("gc_ram_ptr failed");

    uint32_t fails = 0;
    for (uint32_t i = 0; i < (uint32_t)k_mp4_datadir_paths_count; i++) {
        s32 r = DVDConvertPathToEntrynum((char *)k_mp4_datadir_paths[i]);
        if (r == -1) fails++;
    }

    wr32be(p + 0, 0xDEADBEEFu);
    wr32be(p + 4, (uint32_t)k_mp4_datadir_paths_count);
    wr32be(p + 8, fails);
    wr32be(p + 12, (uint32_t)DVDConvertPathToEntrynum("data/does_not_exist.bin"));
}
