#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/gc_host_ram.h"

#include "gc_mem.h"
#include "sdk_state.h"

typedef uint32_t u32;
typedef int32_t s32;

typedef struct { volatile s32 state; } DVDCommandBlock;
typedef struct {
  DVDCommandBlock cb;
  u32 startAddr;
  u32 length;
  s32 entrynum;
} DVDFileInfo;

typedef void (*DVDCallback)(s32 result, DVDFileInfo* fileInfo);

// sdk_port test hooks
void gc_dvd_test_reset_files(void);
void gc_dvd_test_set_file(s32 entrynum, const void *data, u32 len);

// OS / cache / DVD
u32 OSRoundUp32B(u32 x);
u32 OSRoundDown32B(u32 x);

int DVDFastOpen(s32 entrynum, DVDFileInfo *file);
s32 DVDReadAsync(DVDFileInfo *file, void *addr, s32 len, s32 offset, DVDCallback cb);
int DVDGetCommandBlockStatus(DVDCommandBlock *block);
int DVDClose(DVDFileInfo *file);

// We cannot call the real DCInvalidateRange on host. For this smoke chain we
// record the intended invalidate parameters in the snapshot window.
static u32 g_dc_inval_last_addr;
static u32 g_dc_inval_last_len;

static void gc_cache_record_invalidate(u32 gc_addr, u32 nbytes) {
  g_dc_inval_last_addr = gc_addr;
  g_dc_inval_last_len = nbytes;
}

static inline void store_u32be(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)(v >> 0);
}

enum { SNAPSHOT_ADDR = 0x80300100u };

static void emit_snapshot(GcRam *ram) {
  uint8_t *out = gc_ram_ptr(ram, SNAPSHOT_ADDR, 0x40);
  if (!out) return;

  store_u32be(out + 0x00, 0x534D4F4Bu); // "SMOK"
  store_u32be(out + 0x04, 0x00000001u); // version

  // Cache "record-only" surface
  store_u32be(out + 0x08, g_dc_inval_last_addr);
  store_u32be(out + 0x0C, g_dc_inval_last_len);

  // DVD counters
  store_u32be(out + 0x10, gc_sdk_state_load_u32be(GC_SDK_OFF_DVD_READ_CALLS));
  store_u32be(out + 0x14, gc_sdk_state_load_u32be(GC_SDK_OFF_DVD_CLOSE_CALLS));
  store_u32be(out + 0x18, gc_sdk_state_load_u32be(GC_SDK_OFF_DVD_LAST_READ_LEN));
  store_u32be(out + 0x1C, gc_sdk_state_load_u32be(GC_SDK_OFF_DVD_LAST_READ_OFF));
}

int main(void) {
  GcRam ram;
  if (gc_ram_init(&ram, 0x80000000u, 0x01800000u) != 0) return 2;
  gc_mem_set(0x80000000u, 0x01800000u, ram.buf);
  gc_sdk_state_reset();

  // Inject deterministic "disc file" for entry 0.
  static uint8_t k_file[0x100];
  for (u32 i = 0; i < (u32)sizeof(k_file); i++) k_file[i] = (uint8_t)i;

  gc_dvd_test_reset_files();
  gc_dvd_test_set_file(0, k_file, (u32)sizeof(k_file));

  DVDFileInfo fi;
  int ok = DVDFastOpen(0, &fi);

  uint8_t *dest = gc_ram_ptr(&ram, 0x80400000u, 0x100);
  if (dest) memset(dest, 0xAA, 0x100);

  u32 len = 0x23u;
  u32 aligned_up = OSRoundUp32B(len);
  (void)OSRoundDown32B(0x817FFFFFu); // included for MP4 chain parity (not used)

  if (dest) gc_cache_record_invalidate(0x80400000u, aligned_up);
  s32 r = dest ? DVDReadAsync(&fi, dest, (s32)aligned_up, 0, NULL) : 0;
  int st = DVDGetCommandBlockStatus(&fi.cb);
  (void)DVDClose(&fi);

  uint8_t *marker = gc_ram_ptr(&ram, 0x80300000u, 8);
  if (marker) {
    store_u32be(marker + 0x00, 0xC0DEF00Du);
    store_u32be(marker + 0x04, (u32)ok ^ ((u32)r << 8) ^ ((u32)st << 16));
  }
  emit_snapshot(&ram);

  const char *out_rel =
      "tests/sdk/smoke/mp4_hudata_read_chain_001/actual/mp4_hudata_read_chain_001_mem1.bin";
  const char *root = getenv("GC_REPO_ROOT");
  char out_path[4096];
  if (root && root[0]) {
    snprintf(out_path, sizeof(out_path), "%s/%s", root, out_rel);
  } else {
    snprintf(out_path, sizeof(out_path), "%s", out_rel);
  }

  int rc = gc_ram_dump(&ram, 0x80000000u, 0x01800000u, out_path);
  gc_ram_free(&ram);
  return rc;
}
