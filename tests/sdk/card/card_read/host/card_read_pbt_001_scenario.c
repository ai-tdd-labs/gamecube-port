#include <stdint.h>
#include <string.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "dolphin/types.h"
#include "dolphin/exi.h"

#include "sdk_port/card/card_bios.h"
#include "sdk_port/card/memcard_backend.h"

typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t s32;

// Under test.
s32 __CARDReadSegment(s32 chan, void (*callback)(s32, s32));
s32 __CARDRead(s32 chan, u32 addr, s32 length, void* dst, void (*callback)(s32, s32));

extern u32 gc_exi_deselect_calls[3];
extern u32 gc_exi_unlock_calls[3];
extern u32 gc_card_tx_calls[2];

static u32 fnv1a32(const u8* p, u32 n) {
  u32 h = 2166136261u;
  for (u32 i = 0; i < n; i++) {
    h ^= (u32)p[i];
    h *= 16777619u;
  }
  return h;
}

static s32 g_cb_calls;
static s32 g_cb_last;
static void cb_record(s32 chan, s32 result) {
  (void)chan;
  g_cb_calls++;
  g_cb_last = result;
}

const char* gc_scenario_label(void) { return "__CARDRead/pbt_001"; }
const char* gc_scenario_out_path(void) { return "../actual/card_read_pbt_001.bin"; }

void gc_scenario_run(GcRam* ram) {
  u8* out = gc_ram_ptr(ram, 0x80300000u, 0x80u);
  if (!out) die("gc_ram_ptr(out) failed");
  memset(out, 0, 0x80u);
  u32 off = 0;

  // Configure memcard backing store.
  const char* mc_path = "../actual/card_read_pbt_001_memcard.raw";
  const u32 mc_size = 0x40000u;
  if (gc_memcard_insert(0, mc_path, mc_size) != 0) die("gc_memcard_insert failed");

  // Fill image with deterministic bytes, and seed a known read window @0x21000.
  u8 tmp[512];
  for (u32 i = 0; i < mc_size; i += (u32)sizeof(tmp)) {
    for (u32 j = 0; j < (u32)sizeof(tmp); j++) tmp[j] = (u8)((i + j) ^ 0xA5u);
    if (gc_memcard_write(0, i, tmp, (u32)sizeof(tmp)) != 0) die("gc_memcard_write fill failed");
  }
  for (u32 i = 0; i < (u32)sizeof(tmp); i++) tmp[i] = (u8)(0xC0u ^ (u8)i);
  if (gc_memcard_write(0, 0x21000u, tmp, (u32)sizeof(tmp)) != 0) die("gc_memcard_write seed failed");

  // Default setup (mirrors DOL oracle suite assumptions).
  memset(gc_card_block, 0, sizeof(gc_card_block));
  gc_card_block[0].attached = 1;

  // Provide a non-null work area for (workArea + sizeof(CARDID)) pointer math.
  // The host port will treat it as an address, not a struct.
  u8* work = gc_ram_ptr(ram, 0x80400000u, 0x400u);
  if (!work) die("gc_ram_ptr(work) failed");
  memset(work, 0, 0x400u);
  gc_card_block[0].work_area = (uintptr_t)work;
  gc_card_block[0].latency = 0;

  EXIInit();
  gc_exi_probeex_ret[0] = 1; // device present so __CARDTxHandler returns READY
  gc_exi_dma_hook = gc_memcard_exi_dma;
  gc_exi_deselect_calls[0] = 0;
  gc_exi_unlock_calls[0] = 0;
  gc_card_tx_calls[0] = 0;

  // Case A: __CARDReadSegment success.
  {
    u8* buf = gc_ram_ptr(ram, 0x80401000u, 512u);
    if (!buf) die("gc_ram_ptr(bufA) failed");
    memset(buf, 0x11, 512u);
    gc_card_block[0].addr = 0x21000u;
    gc_card_block[0].buffer = (uintptr_t)buf;

    g_cb_calls = 0;
    g_cb_last = 0;

    s32 rc = __CARDReadSegment(0, cb_record);
    u32 h = fnv1a32(buf, 512u);

    wr32be(out + off, 0x43524441u); off += 4;
    wr32be(out + off, (u32)rc); off += 4;
    wr32be(out + off, (u32)g_cb_calls); off += 4;
    wr32be(out + off, (u32)g_cb_last); off += 4;
    wr32be(out + off, h); off += 4;
  }

  // Case B: __CARDRead chains 2 segments.
  {
    u8* buf = gc_ram_ptr(ram, 0x80402000u, 1024u);
    if (!buf) die("gc_ram_ptr(bufB) failed");
    memset(buf, 0x22, 1024u);
    gc_card_block[0].xferred = 0;

    g_cb_calls = 0;
    g_cb_last = 0;

    s32 rc = __CARDRead(0, 0x21000u, 1024, buf, cb_record);
    u32 h0 = fnv1a32(buf + 0, 512u);
    u32 h1 = fnv1a32(buf + 512u, 512u);

    wr32be(out + off, 0x43524442u); off += 4;
    wr32be(out + off, (u32)rc); off += 4;
    wr32be(out + off, (u32)g_cb_calls); off += 4;
    wr32be(out + off, (u32)g_cb_last); off += 4;
    wr32be(out + off, (u32)gc_card_block[0].xferred); off += 4;
    wr32be(out + off, h0); off += 4;
    wr32be(out + off, h1); off += 4;
  }

  // Case C: BUSY lock path (host EXI lock fail via state pre-lock).
  {
    u8* buf = gc_ram_ptr(ram, 0x80403000u, 512u);
    if (!buf) die("gc_ram_ptr(bufC) failed");
    memset(buf, 0xEE, 512u);
    u32 h_before = fnv1a32(buf, 512u);

    // Force EXILock to fail by pre-locking the channel.
    (void)EXILock(0, 0, 0);

    gc_card_block[0].addr = 0x21000u;
    gc_card_block[0].buffer = (uintptr_t)buf;
    g_cb_calls = 0;
    g_cb_last = 0;

    s32 rc = __CARDReadSegment(0, cb_record);
    u32 h_after = fnv1a32(buf, 512u);

    wr32be(out + off, 0x43524443u); off += 4;
    wr32be(out + off, (u32)rc); off += 4;
    wr32be(out + off, (u32)g_cb_calls); off += 4;
    wr32be(out + off, h_before); off += 4;
    wr32be(out + off, h_after); off += 4;

    // Clean up the forced lock so later tests don't inherit it.
    (void)EXIUnlock(0);
  }

  // Case D: DMA fail (disable hook), check cleanup counters.
  {
    u8* buf = gc_ram_ptr(ram, 0x80404000u, 512u);
    if (!buf) die("gc_ram_ptr(bufD) failed");
    memset(buf, 0xDD, 512u);

    EXIInit();
    gc_exi_probeex_ret[0] = 1;
    gc_exi_dma_hook = 0;
    gc_exi_deselect_calls[0] = 0;
    gc_exi_unlock_calls[0] = 0;
    gc_card_tx_calls[0] = 0;

    gc_card_block[0].addr = 0x21000u;
    gc_card_block[0].buffer = (uintptr_t)buf;
    g_cb_calls = 0;

    s32 rc = __CARDReadSegment(0, cb_record);

    wr32be(out + off, 0x43524444u); off += 4;
    wr32be(out + off, (u32)rc); off += 4;
    wr32be(out + off, gc_exi_deselect_calls[0]); off += 4;
    wr32be(out + off, gc_exi_unlock_calls[0]); off += 4;
    wr32be(out + off, gc_card_tx_calls[0]); off += 4;
    wr32be(out + off, (u32)g_cb_calls); off += 4;
  }

  wr32be(out + 0x00, 0x43524430u);
}
