#include "gc_host_scenario.h"
#include "gc_mem.h"

#include "card/memcard_backend.h"

#include <stdint.h>
#include <stdio.h>

typedef uint32_t u32;

static inline void be_store(u32 addr, u32 v) {
  uint8_t* p = gc_mem_ptr(addr, 4);
  if (!p) return;
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)v;
}

const char* gc_scenario_label(void) { return "CARD/memcard_backend_unit_001"; }

const char* gc_scenario_out_path(void) { return "../actual/memcard_backend_unit_001.bin"; }

void gc_scenario_run(GcRam* ram) {
  (void)ram;

  // Put the temp image under actual/ (ignored).
  const char* path = "../actual/memcard_backend_unit_001.raw";
  const u32 size = 0x2000u;  // small for unit test

  int rc_ins = gc_memcard_insert(0, path, size);
  int ins0 = gc_memcard_is_inserted(0);
  u32 sz0 = gc_memcard_size(0);

  uint8_t pat[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};
  int rc_wr = gc_memcard_write(0, 0x10u, pat, sizeof(pat));
  int rc_fl = gc_memcard_flush(0);

  uint8_t rd[8] = {0};
  int rc_rd = gc_memcard_read(0, 0x10u, rd, sizeof(rd));

  // Output
  u32 out = 0x80300000u;
  u32 off = 0;
  be_store(out + off * 4u, 0x4D434231u); off++; // MCB1
  be_store(out + off * 4u, (u32)rc_ins); off++;
  be_store(out + off * 4u, (u32)ins0); off++;
  be_store(out + off * 4u, (u32)sz0); off++;
  be_store(out + off * 4u, (u32)rc_wr); off++;
  be_store(out + off * 4u, (u32)rc_fl); off++;
  be_store(out + off * 4u, (u32)rc_rd); off++;
  be_store(out + off * 4u, ((u32)rd[0] << 24) | ((u32)rd[1] << 16) | ((u32)rd[2] << 8) | (u32)rd[3]); off++;
  be_store(out + off * 4u, ((u32)rd[4] << 24) | ((u32)rd[5] << 16) | ((u32)rd[6] << 8) | (u32)rd[7]); off++;
}
