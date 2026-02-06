#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef uint16_t u16;

float GXGetYScaleFactor(u16 efbHeight, u16 xfbHeight);

const char *gc_scenario_label(void) { return "GXGetYScaleFactor/mp4_init_gx"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_get_y_scale_factor_mp4_init_gx_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    float f = GXGetYScaleFactor(480, 528);
    union { float f; u32 u; } u = { .f = f };

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 8);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, u.u);
}
