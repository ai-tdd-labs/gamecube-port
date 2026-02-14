#include <stdint.h>
#include <string.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "dolphin/types.h"
#include "dolphin/exi.h"

#include "sdk_port/card/memcard_backend.h"

static inline uint32_t rotl1(uint32_t v) { return (v << 1) | (v >> 31); }

static void be(uint8_t *p, uint32_t v) { wr32be(p, v); }

static uint32_t hash_bytes(const uint8_t *p, uint32_t n) {
  uint32_t h = 0;
  for (uint32_t i = 0; i < n; i++) h = rotl1(h) ^ (uint32_t)p[i];
  return h;
}

static void encode_cmd(uint8_t *cmd5, uint8_t op, uint32_t addr) {
  cmd5[0] = op;
  cmd5[1] = (uint8_t)((addr >> 17) & 0x7Fu);
  cmd5[2] = (uint8_t)((addr >> 9) & 0xFFu);
  cmd5[3] = (uint8_t)((addr >> 7) & 0x03u);
  cmd5[4] = (uint8_t)(addr & 0x7Fu);
}

const char *gc_scenario_label(void) { return "exi_dma_card_proto_pbt_001"; }
const char *gc_scenario_out_path(void) { return "../actual/exi_dma_card_proto_pbt_001.bin"; }

void gc_scenario_run(GcRam *ram) {
  uint8_t *out = gc_ram_ptr(ram, 0x80300000u, 0x40);
  uint32_t off = 0;

  // Configure memcard backing store.
  const char *mc_path = "../actual/exi_dma_card_proto_pbt_001_memcard.raw";
  const uint32_t mc_size = 0x40000u;
  if (gc_memcard_insert(0, mc_path, mc_size) != 0) die("gc_memcard_insert failed");

  // Fill image with deterministic bytes, and seed a known read window @0x21000.
  uint8_t tmp[512];
  for (uint32_t i = 0; i < mc_size; i += (uint32_t)sizeof(tmp)) {
    for (uint32_t j = 0; j < (uint32_t)sizeof(tmp); j++) tmp[j] = (uint8_t)((i + j) ^ 0xA5u);
    if (gc_memcard_write(0, i, tmp, (uint32_t)sizeof(tmp)) != 0) die("gc_memcard_write fill failed");
  }
  for (uint32_t i = 0; i < (uint32_t)sizeof(tmp); i++) tmp[i] = (uint8_t)(0xC0u ^ (uint8_t)i);
  if (gc_memcard_write(0, 0x21000u, tmp, (uint32_t)sizeof(tmp)) != 0) die("gc_memcard_write seed failed");

  EXIInit();
  gc_exi_dma_hook = gc_memcard_exi_dma;

  if (!EXILock(0, 0, 0)) die("EXILock failed");
  if (!EXISelect(0, 0, 4)) die("EXISelect failed");

  // Read 512 bytes @0x21000 (exercises cmd1 nonzero).
  uint8_t cmd[5];
  encode_cmd(cmd, 0x52, 0x21000u);
  if (!EXIImmEx(0, cmd, 5, EXI_WRITE)) die("EXIImmEx(cmd read) failed");
  uint8_t buf[512];
  memset(buf, 0, sizeof(buf));
  int rd_ok = EXIDma(0, buf, (s32)sizeof(buf), EXI_READ, 0);
  uint32_t rd_h = hash_bytes(buf, (uint32_t)sizeof(buf));

  // Write 128 bytes @0x22000 then read-back via backend.
  encode_cmd(cmd, 0xF2, 0x22000u);
  if (!EXIImmEx(0, cmd, 5, EXI_WRITE)) die("EXIImmEx(cmd write) failed");
  uint8_t wr[128];
  for (uint32_t i = 0; i < (uint32_t)sizeof(wr); i++) wr[i] = (uint8_t)(0x5Au ^ (uint8_t)i);
  int wr_ok = EXIDma(0, wr, (s32)sizeof(wr), EXI_WRITE, 0);
  uint8_t back[128];
  memset(back, 0, sizeof(back));
  int bk_ok = (gc_memcard_read(0, 0x22000u, back, (uint32_t)sizeof(back)) == 0);
  uint32_t bk_h = hash_bytes(back, (uint32_t)sizeof(back));

  EXIDeselect(0);
  EXIUnlock(0);

  be(out + off * 4u, 0x5844494Du); off++;
  be(out + off * 4u, (uint32_t)rd_ok); off++;
  be(out + off * 4u, rd_h); off++;
  be(out + off * 4u, (uint32_t)wr_ok); off++;
  be(out + off * 4u, (uint32_t)bk_ok); off++;
  be(out + off * 4u, bk_h); off++;

  // Keep the file deterministic for later suites by flushing after writes.
  (void)gc_memcard_flush(0);
}
