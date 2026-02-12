#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;

void GXResetWriteGatherPipe(void);

extern u32 gc_gx_bp_sent_not;

const char *gc_scenario_label(void) { return "gx_reset_write_gather_pipe/mp4_hsfdraw"; }
const char *gc_scenario_out_path(void) {
    return "../actual/gx_reset_write_gather_pipe_mp4_hsfdraw_001.bin";
}

void gc_scenario_run(GcRam *ram) {
    gc_gx_bp_sent_not = 0x11223344u;
    GXResetWriteGatherPipe();

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x08);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_bp_sent_not);
}
