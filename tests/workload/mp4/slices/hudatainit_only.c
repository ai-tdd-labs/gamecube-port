#include <stdint.h>

// Minimal, host-buildable slice of MP4 decomp: HuDataInit (data.c).
//
// Purpose: reachability/integration only (NOT a correctness oracle).
//
// Real MP4 HuDataInit walks a large table of data paths and panics if any path
// is missing from the disc FST. Our host runtime does not model the full disc
// filesystem yet, so we seed the sdk_port "test backend" path map with a small
// subset of MP4 paths and validate lookups without hard-crashing.

typedef int32_t s32;

// sdk_port dvd test backend + API.
void gc_dvd_test_reset_paths(void);
void gc_dvd_test_set_paths(const char **paths, s32 count);
s32 DVDConvertPathToEntrynum(char *pathPtr);

// Minimal error reporting (sdk_port OSReport is available in workload builds).
void OSReport(const char *fmt, ...);

static const char *k_mp4_data_paths[] = {
    "data/E3setup.bin",
    "data/bbattle.bin",
    "data/bguest.bin",
    "data/bkoopa.bin",
    "data/bkoopasuit.bin",
};

void HuDataInit(void) {
    gc_dvd_test_reset_paths();
    gc_dvd_test_set_paths(k_mp4_data_paths, (s32)(sizeof(k_mp4_data_paths) / sizeof(k_mp4_data_paths[0])));

    for (s32 i = 0; i < (s32)(sizeof(k_mp4_data_paths) / sizeof(k_mp4_data_paths[0])); i++) {
        s32 entry = DVDConvertPathToEntrynum((char *)k_mp4_data_paths[i]);
        if (entry < 0) {
            OSReport("HuDataInit(host): missing data path %s\n", k_mp4_data_paths[i]);
        }
    }
}

