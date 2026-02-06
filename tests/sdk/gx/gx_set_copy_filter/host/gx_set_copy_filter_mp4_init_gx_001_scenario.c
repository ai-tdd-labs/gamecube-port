#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

extern uint32_t gc_gx_copy_filter_aa;
extern uint32_t gc_gx_copy_filter_vf;
extern uint32_t gc_gx_copy_filter_sample_hash;
extern uint32_t gc_gx_copy_filter_vfilter_hash;

typedef uint32_t u32;
typedef uint8_t u8;

void GXSetCopyFilter(u8 aa, const u8 sample_pattern[12][2], u8 vf, const u8 vfilter[7]);

const char *gc_scenario_label(void) { return "GXSetCopyFilter/mp4_init_gx"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_copy_filter_mp4_init_gx_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    static const u8 sample_pattern[12][2] = {
        {6, 6}, {6, 6}, {6, 6}, {6, 6},
        {6, 6}, {6, 6}, {6, 6}, {6, 6},
        {6, 6}, {6, 6}, {6, 6}, {6, 6},
    };
    static const u8 vfilter[7] = {0, 0, 21, 22, 21, 0, 0};

    GXSetCopyFilter(0, sample_pattern, 1, vfilter);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x14);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_copy_filter_aa);
    wr32be(p + 0x08, gc_gx_copy_filter_vf);
    wr32be(p + 0x0C, gc_gx_copy_filter_sample_hash);
    wr32be(p + 0x10, gc_gx_copy_filter_vfilter_hash);
}
