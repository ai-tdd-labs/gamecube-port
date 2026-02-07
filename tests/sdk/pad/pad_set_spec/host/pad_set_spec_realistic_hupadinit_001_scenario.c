#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "sdk_state.h"

void PADSetSpec(uint32_t spec);

enum {
  PAD_SPEC_5 = 5,
  EXPECT_KIND_SPEC2 = 2,
};

const char *gc_scenario_label(void) { return "PADSetSpec/mp4_realistic_hupadinit"; }
const char *gc_scenario_out_path(void) { return "../actual/pad_set_spec_realistic_hupadinit_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    PADSetSpec(PAD_SPEC_5);

    uint32_t spec = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_SPEC);
    uint32_t kind = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MAKE_STATUS_KIND);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x10);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, spec);
    wr32be(p + 0x08, kind);
    wr32be(p + 0x0C, EXPECT_KIND_SPEC2);
}
