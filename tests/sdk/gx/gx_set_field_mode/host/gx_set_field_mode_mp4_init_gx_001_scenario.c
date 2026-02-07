#include <stdint.h>
#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"
void GXSetFieldMode(uint32_t field_mode, uint32_t half_aspect);
extern uint32_t gc_gx_field_mode_field_mode;
extern uint32_t gc_gx_field_mode_half_aspect;
const char *gc_scenario_label(void) { return "GXSetFieldMode/mp4_init_gx"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_field_mode_mp4_init_gx_001.bin"; }
void gc_scenario_run(GcRam *ram) {
  GXSetFieldMode(0, 0);
  uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x0C);
  if (!p) die("gc_ram_ptr failed");
  wr32be(p + 0x00, 0xDEADBEEFu);
  wr32be(p + 0x04, gc_gx_field_mode_field_mode);
  wr32be(p + 0x08, gc_gx_field_mode_half_aspect);
}
