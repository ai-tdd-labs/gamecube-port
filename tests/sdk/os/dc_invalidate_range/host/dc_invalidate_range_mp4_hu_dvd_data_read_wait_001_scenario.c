#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;

extern u32 gc_dc_inval_last_addr;
extern u32 gc_dc_inval_last_len;
void DCInvalidateRange(void *addr, u32 nbytes);

const char *gc_scenario_label(void) { return "DCInvalidateRange/mp4_hu_dvd_data_read_wait"; }
const char *gc_scenario_out_path(void) { return "../actual/dc_invalidate_range_mp4_hu_dvd_data_read_wait_001.bin"; }

void gc_scenario_run(GcRam *ram) {
  (void)ram;
  gc_dc_inval_last_addr = 0;
  gc_dc_inval_last_len = 0;

  void *buf = (void *)0x80400000u;
  u32 len = 0x40;
  DCInvalidateRange(buf, len);

  uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x0C);
  if (!p) die("gc_ram_ptr failed");
  wr32be(p + 0x00, 0xDEADBEEFu);
  wr32be(p + 0x04, gc_dc_inval_last_addr);
  wr32be(p + 0x08, gc_dc_inval_last_len);
}

