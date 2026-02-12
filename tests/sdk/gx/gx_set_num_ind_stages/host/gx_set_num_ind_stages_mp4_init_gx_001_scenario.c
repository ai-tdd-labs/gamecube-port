#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint8_t u8;
typedef uint32_t u32;

void GXSetNumIndStages(u8 nIndStages);

extern u32 gc_gx_gen_mode;
extern u32 gc_gx_dirty_state;

const char *gc_scenario_label(void) { return "gx_set_num_ind_stages/mp4_init_gx"; }
const char *gc_scenario_out_path(void) {
    return "../actual/gx_set_num_ind_stages_mp4_init_gx_001.bin";
}

void gc_scenario_run(GcRam *ram) {
    gc_gx_gen_mode = 0xA5A50000u;
    gc_gx_dirty_state = 0x00000008u;

    GXSetNumIndStages(3u);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x0C);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_gen_mode);
    wr32be(p + 0x08, gc_gx_dirty_state);
}
