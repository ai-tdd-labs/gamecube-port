#include <stdint.h>
#include <string.h>
#include "src/sdk_port/gc_mem.c"

typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t s32;

// Oracle API.
void oracle_mount_init_defaults(void);
void oracle_mount_seed_valid_card(int chan);

extern s32 oracle_exi_probeex_ret[3];
extern u32 oracle_exi_getid_ok[3];
extern u32 oracle_exi_id[3];
extern u32 oracle_exi_card_status[3];

typedef void (*CARDCallback)(s32 chan, s32 result);
s32 oracle_CARDMountAsync(s32 chan, void* workArea, CARDCallback detachCallback, CARDCallback attachCallback);

typedef struct {
  int attached;
  s32 result;
  uint16_t size;
  s32 sectorSize;
  uint16_t cBlock;
  s32 latency;
  u8 id[12];
  int mountStep;
  void* workArea;
  void* currentDir;
  void* currentFat;
  CARDCallback extCallback;
  CARDCallback apiCallback;
  CARDCallback exiCallback;
  CARDCallback unlockCallback;
  u32 cid;
} CARDControl;
extern CARDControl __CARDBlock[2];

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

static s32 g_attach_calls;
static s32 g_attach_last;
static void attach_cb(s32 chan, s32 result) {
  (void)chan;
  g_attach_calls++;
  g_attach_last = result;
}

int main(void) {
  gc_mem_set(0x80000000u, 0x01800000u, (u8*)0x80000000u);

  volatile u8* out = (volatile u8*)0x80300000u;
  for (u32 i = 0; i < 0x100u; i++) out[i] = 0;
  u32 off = 0;

  // Ensure GameChoice doesn't block mounts.
  volatile u8* gamechoice = (volatile u8*)0x800030E3u;
  *gamechoice = 0;

  oracle_mount_init_defaults();
  oracle_mount_seed_valid_card(0);

  // Configure EXI to present a 4Mb card with sectorSize index 0 (8KiB).
  oracle_exi_probeex_ret[0] = 1;
  oracle_exi_getid_ok[0] = 1;
  oracle_exi_id[0] = 0x00000004u;

  // status bit 0x40 set => skip unlock path.
  oracle_exi_card_status[0] = 0x40u;

  static u8 work[5 * 8 * 1024];
  memset(work, 0, sizeof(work));

  memset(__CARDBlock, 0, sizeof(__CARDBlock));
  __CARDBlock[0].attached = 1;
  __CARDBlock[0].workArea = work;
  __CARDBlock[0].currentDir = 0;
  __CARDBlock[0].currentFat = 0;

  g_attach_calls = 0;
  g_attach_last = 0;
  s32 rc = oracle_CARDMountAsync(0, work, attach_cb, attach_cb);

  u32 h0 = fnv1a32(work + 0x0000u, 0x2000u);
  u32 h1 = fnv1a32(work + 0x2000u, 0x2000u);
  u32 h2 = fnv1a32(work + 0x4000u, 0x2000u);
  u32 h3 = fnv1a32(work + 0x6000u, 0x2000u);
  u32 h4 = fnv1a32(work + 0x8000u, 0x2000u);

  wr32be(out + off, 0x434D5430u); off += 4; // "CMT0"
  wr32be(out + off, (u32)rc); off += 4;
  wr32be(out + off, (u32)__CARDBlock[0].mountStep); off += 4;
  wr32be(out + off, (u32)__CARDBlock[0].result); off += 4;
  wr32be(out + off, (u32)g_attach_calls); off += 4;
  wr32be(out + off, (u32)g_attach_last); off += 4;
  wr32be(out + off, h0); off += 4;
  wr32be(out + off, h1); off += 4;
  wr32be(out + off, h2); off += 4;
  wr32be(out + off, h3); off += 4;
  wr32be(out + off, h4); off += 4;

  while (1) { __asm__ volatile("nop"); }
  return 0;
}

