#include <stdint.h>
#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"
void GXPokeBlendMode(uint32_t type, uint32_t src, uint32_t dst, uint32_t op);
extern uint32_t gc_gx_poke_blend_type;
extern uint32_t gc_gx_poke_blend_src;
extern uint32_t gc_gx_poke_blend_dst;
extern uint32_t gc_gx_poke_blend_op;
const char *gc_scenario_label(void) { return "GXPokeBlendMode/mp4_init_gx"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_poke_blend_mode_mp4_init_gx_001.bin"; }
void gc_scenario_run(GcRam *ram) {
  GXPokeBlendMode(0, 0, 1, 15);
  uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x20);
  if (!p) die("gc_ram_ptr failed");
  wr32be(p + 0x00, 0xDEADBEEFu);
  wr32be(p + 0x04, gc_gx_poke_blend_type);
  wr32be(p + 0x08, gc_gx_poke_blend_src);
  wr32be(p + 0x0C, gc_gx_poke_blend_dst);
  wr32be(p + 0x10, gc_gx_poke_blend_op);
}
