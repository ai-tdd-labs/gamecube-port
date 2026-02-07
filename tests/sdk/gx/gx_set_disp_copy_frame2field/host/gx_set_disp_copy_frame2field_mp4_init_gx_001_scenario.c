#include <stdint.h>
#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"
void GXSetDispCopyFrame2Field(uint32_t mode);
extern uint32_t gc_gx_copy_frame2field;
const char *gc_scenario_label(void) { return "GXSetDispCopyFrame2Field/mp4_init_gx"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_disp_copy_frame2field_mp4_init_gx_001.bin"; }
void gc_scenario_run(GcRam *ram) {
  GXSetDispCopyFrame2Field(0);
  uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x08);
  if (!p) die("gc_ram_ptr failed");
  wr32be(p + 0x00, 0xDEADBEEFu);
  wr32be(p + 0x04, gc_gx_copy_frame2field);
}
