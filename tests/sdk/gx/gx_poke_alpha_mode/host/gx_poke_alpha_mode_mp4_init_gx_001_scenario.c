#include <stdint.h>
#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"
void GXPokeAlphaMode(uint32_t func, uint32_t threshold);
extern uint32_t gc_gx_poke_alpha_mode_func;
extern uint32_t gc_gx_poke_alpha_mode_thresh;
const char *gc_scenario_label(void) { return "GXPokeAlphaMode/mp4_init_gx"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_poke_alpha_mode_mp4_init_gx_001.bin"; }
void gc_scenario_run(GcRam *ram) {
  GXPokeAlphaMode(7, 0);
  uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x0C);
  if (!p) die("gc_ram_ptr failed");
  wr32be(p + 0x00, 0xDEADBEEFu);
  wr32be(p + 0x04, gc_gx_poke_alpha_mode_func);
  wr32be(p + 0x08, gc_gx_poke_alpha_mode_thresh);
}
