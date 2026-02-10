#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/gc_host_ram.h"

#include "gc_mem.h"
#include "sdk_state.h"

static inline void store_u32be(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)(v >> 0);
}

enum { SNAPSHOT_ADDR = 0x80300100u };

// OS
void OSInit(void);
void OSSetArenaLo(void *addr);
void OSSetArenaHi(void *addr);

// Stopwatch
typedef struct OSStopwatch {
  char *name;
  uint64_t total;
  uint32_t hits;
  uint32_t running;
  uint64_t last;
  uint64_t min;
  uint64_t max;
} OSStopwatch;

void OSInitStopwatch(OSStopwatch *sw, char *name);
void OSStartStopwatch(OSStopwatch *sw);
void OSStopStopwatch(OSStopwatch *sw);
long long OSCheckStopwatch(OSStopwatch *sw);
void OSResetStopwatch(OSStopwatch *sw);

// GX perf sync
typedef void (*GXDrawSyncCallback)(uint16_t token);
GXDrawSyncCallback GXSetDrawSyncCallback(GXDrawSyncCallback cb);
void GXSetDrawSync(uint16_t token);

// We use a real function pointer so sdk_port can store something, but we do NOT
// compare the pointer value (host vs PPC differ).
static void DrawSyncCB(uint16_t token) { (void)token; }

static void emit_snapshot(GcRam *ram, const OSStopwatch *sw) {
  uint8_t *out = gc_ram_ptr(ram, SNAPSHOT_ADDR, 0x40);
  if (!out) return;

  store_u32be(out + 0x00, 0x534D4F4Bu); // "SMOK"
  store_u32be(out + 0x04, 0x00000001u); // version

  // Stopwatch observable results (32-bit truncation is fine for our deterministic counter).
  store_u32be(out + 0x08, (uint32_t)(sw->total & 0xFFFFFFFFu));
  store_u32be(out + 0x0C, sw->hits);
  store_u32be(out + 0x10, sw->running);
  store_u32be(out + 0x14, (uint32_t)(sw->min & 0xFFFFFFFFu));
  store_u32be(out + 0x18, (uint32_t)(sw->max & 0xFFFFFFFFu));

  // GX observable state.
  extern uint32_t gc_gx_last_draw_sync_token;
  store_u32be(out + 0x1C, gc_gx_last_draw_sync_token);

  // Also include the last BP/RAS value as an extra signal.
  extern uint32_t gc_gx_last_ras_reg;
  store_u32be(out + 0x20, gc_gx_last_ras_reg);

  // Keep the rest reserved/zero for now.
  for (uint32_t off = 0x24; off < 0x40; off += 4) {
    store_u32be(out + off, 0);
  }
}

int main(void) {
  GcRam ram;
  if (gc_ram_init(&ram, 0x80000000u, 0x01800000u) != 0) return 2;
  gc_mem_set(0x80000000u, 0x01800000u, ram.buf);
  gc_sdk_state_reset();

  OSInit();
  // Keep host-vs-PPC comparisons deterministic.
  OSSetArenaLo((void *)0x80026000u);
  OSSetArenaHi((void *)0x81700000u);

  // Perf chain (MP4 HuPerfInit/HuPerfBegin-style primitives).
  OSStopwatch sw;
  OSInitStopwatch(&sw, NULL);
  OSResetStopwatch(&sw);
  OSStartStopwatch(&sw);

  // Set/clear callback just to exercise storage.
  (void)GXSetDrawSyncCallback(DrawSyncCB);
  (void)GXSetDrawSyncCallback(NULL);

  // One token write; MP4 uses this for perf sync points.
  GXSetDrawSync(0x1234);

  OSStopStopwatch(&sw);
  (void)OSCheckStopwatch(&sw);

  // Marker in big-endian to match Dolphin RAM dumps.
  uint8_t *marker = gc_ram_ptr(&ram, 0x80300000u, 8);
  if (marker) {
    store_u32be(marker + 0x00, 0xC0DEF00Du);
    store_u32be(marker + 0x04, 0x00000020u);
  }
  emit_snapshot(&ram, &sw);

  const char *out_rel = "tests/sdk/smoke/mp4_perf_chain_001/actual/mp4_perf_chain_001_mem1.bin";
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

