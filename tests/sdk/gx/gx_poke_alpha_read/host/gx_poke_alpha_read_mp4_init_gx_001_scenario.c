#include <stdint.h>
#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"
void GXPokeAlphaRead(uint32_t mode);
extern uint32_t gc_gx_poke_alpha_read_mode;
const char *gc_scenario_label(void) { return "GXPokeAlphaRead/mp4_init_gx"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_poke_alpha_read_mp4_init_gx_001.bin"; }
void gc_scenario_run(GcRam *ram) {
  GXPokeAlphaRead(1);
  uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x08);
  if (!p) die("gc_ram_ptr failed");
  wr32be(p + 0x00, 0xDEADBEEFu);
  wr32be(p + 0x04, gc_gx_poke_alpha_read_mode);
}
