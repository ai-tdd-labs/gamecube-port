#include <stdint.h>
#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"
void GXPokeZMode(uint32_t enable, uint32_t func, uint32_t update);
extern uint32_t gc_gx_poke_zmode_enable;
extern uint32_t gc_gx_poke_zmode_func;
extern uint32_t gc_gx_poke_zmode_update_enable;
const char *gc_scenario_label(void) { return "GXPokeZMode/mp4_init_gx"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_poke_z_mode_mp4_init_gx_001.bin"; }
void gc_scenario_run(GcRam *ram) {
  GXPokeZMode(1, 7, 1);
  uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x14);
  if (!p) die("gc_ram_ptr failed");
  wr32be(p + 0x00, 0xDEADBEEFu);
  wr32be(p + 0x04, gc_gx_poke_zmode_enable);
  wr32be(p + 0x08, gc_gx_poke_zmode_func);
  wr32be(p + 0x0C, gc_gx_poke_zmode_update_enable);
}
