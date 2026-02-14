#include <stdint.h>
#include <string.h>
#include "src/sdk_port/gc_mem.c"

typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t s32;

// Oracle API.
void oracle_EXIInit(void);
extern u32 oracle_exi_lock_ok;
extern u32 oracle_exi_select_ok;
extern u32 oracle_exi_dma_ok;
extern u32 oracle_exi_probe_ok;
extern u32 oracle_exi_deselect_calls;
extern u32 oracle_exi_unlock_calls;
extern u32 oracle_card_tx_calls;

typedef void (*CARDCallback)(s32 chan, s32 result);
typedef struct {
  int attached;
  s32 result;
  s32 latency;
  void* workArea;
  u8 cmd[9];
  s32 cmdlen;
  u32 mode;
  int retry;
  int repeat;
  u32 addr;
  void* buffer;
  s32 xferred;
  CARDCallback txCallback;
  CARDCallback exiCallback;
  CARDCallback apiCallback;
  CARDCallback xferCallback;
  CARDCallback unlockCallback;
} CARDControl;
extern CARDControl __CARDBlock[2];

s32 oracle___CARDReadSegment(s32 chan, CARDCallback callback);
s32 oracle___CARDRead(s32 chan, u32 addr, s32 length, void* dst, CARDCallback callback);

static inline void wr32be(volatile u8* p, u32 v) {
  p[0] = (u8)(v >> 24);
  p[1] = (u8)(v >> 16);
  p[2] = (u8)(v >> 8);
  p[3] = (u8)v;
}

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

int main(void) {
  gc_mem_set(0x80000000u, 0x01800000u, (u8*)0x80000000u);

  volatile u8* out = (volatile u8*)0x80300000u;
  for (u32 i = 0; i < 0x80u; i++) out[i] = 0;
  u32 off = 0;

  oracle_EXIInit();
  memset(__CARDBlock, 0, sizeof(__CARDBlock));
  static u8 work[1024];
  __CARDBlock[0].attached = 1;
  __CARDBlock[0].workArea = work;

  // Case A: __CARDReadSegment success, latency=0 (so cmd is last EXIImmEx side effect).
  {
    u8 buf[512];
    memset(buf, 0x11, sizeof(buf));
    __CARDBlock[0].latency = 0;
    __CARDBlock[0].addr = 0x21000u;
    __CARDBlock[0].buffer = buf;

    oracle_exi_lock_ok = 1;
    oracle_exi_select_ok = 1;
    oracle_exi_dma_ok = 1;
    oracle_exi_probe_ok = 1;
    g_cb_calls = 0;
    g_cb_last = 0;

    s32 rc = oracle___CARDReadSegment(0, cb_record);
    u32 h = fnv1a32(buf, (u32)sizeof(buf));

    wr32be(out + off, 0x43524441u); off += 4; // "CRDA"
    wr32be(out + off, (u32)rc); off += 4;
    wr32be(out + off, (u32)g_cb_calls); off += 4;
    wr32be(out + off, (u32)g_cb_last); off += 4;
    wr32be(out + off, h); off += 4;
  }

  // Case B: __CARDRead chains 2 segments (1024 bytes).
  {
    u8 buf[1024];
    memset(buf, 0x22, sizeof(buf));
    __CARDBlock[0].xferred = 0;
    __CARDBlock[0].latency = 0;

    oracle_exi_lock_ok = 1;
    oracle_exi_select_ok = 1;
    oracle_exi_dma_ok = 1;
    oracle_exi_probe_ok = 1;
    g_cb_calls = 0;
    g_cb_last = 0;

    s32 rc = oracle___CARDRead(0, 0x21000u, 1024, buf, cb_record);
    u32 h0 = fnv1a32(buf + 0, 512u);
    u32 h1 = fnv1a32(buf + 512u, 512u);

    wr32be(out + off, 0x43524442u); off += 4; // "CRDB"
    wr32be(out + off, (u32)rc); off += 4;
    wr32be(out + off, (u32)g_cb_calls); off += 4;
    wr32be(out + off, (u32)g_cb_last); off += 4;
    wr32be(out + off, (u32)__CARDBlock[0].xferred); off += 4;
    wr32be(out + off, h0); off += 4;
    wr32be(out + off, h1); off += 4;
  }

  // Case C: BUSY lock -> __CARDReadSegment returns READY, no IO, callback not invoked.
  {
    u8 buf[512];
    memset(buf, 0xEE, sizeof(buf));
    u32 h_before = fnv1a32(buf, (u32)sizeof(buf));

    __CARDBlock[0].latency = 0;
    __CARDBlock[0].addr = 0x21000u;
    __CARDBlock[0].buffer = buf;

    oracle_exi_lock_ok = 0;
    oracle_exi_select_ok = 1;
    oracle_exi_dma_ok = 1;
    oracle_exi_probe_ok = 1;
    g_cb_calls = 0;
    g_cb_last = 0;

    s32 rc = oracle___CARDReadSegment(0, cb_record);
    u32 h_after = fnv1a32(buf, (u32)sizeof(buf));

    wr32be(out + off, 0x43524443u); off += 4; // "CRDC"
    wr32be(out + off, (u32)rc); off += 4;
    wr32be(out + off, (u32)g_cb_calls); off += 4;
    wr32be(out + off, h_before); off += 4;
    wr32be(out + off, h_after); off += 4;
  }

  // Case D: DMA fail -> NOCARD, cleanup path observable via deselect/unlock counters.
  {
    u8 buf[512];
    memset(buf, 0xDD, sizeof(buf));

    __CARDBlock[0].latency = 0;
    __CARDBlock[0].addr = 0x21000u;
    __CARDBlock[0].buffer = buf;

    oracle_EXIInit(); // reset counters deterministically
    oracle_exi_lock_ok = 1;
    oracle_exi_select_ok = 1;
    oracle_exi_dma_ok = 0;
    oracle_exi_probe_ok = 1;
    g_cb_calls = 0;
    g_cb_last = 0;

    s32 rc = oracle___CARDReadSegment(0, cb_record);

    wr32be(out + off, 0x43524444u); off += 4; // "CRDD"
    wr32be(out + off, (u32)rc); off += 4;
    wr32be(out + off, oracle_exi_deselect_calls); off += 4;
    wr32be(out + off, oracle_exi_unlock_calls); off += 4;
    wr32be(out + off, oracle_card_tx_calls); off += 4;
    wr32be(out + off, (u32)g_cb_calls); off += 4;
  }

  wr32be(out + 0x00, 0x43524430u); // "CRD0" suite marker

  while (1) { __asm__ volatile("nop"); }
  return 0;
}

