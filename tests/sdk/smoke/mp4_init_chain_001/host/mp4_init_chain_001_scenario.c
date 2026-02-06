#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "gc_host_ram.h"
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
void *OSGetArenaLo(void);
void *OSGetArenaHi(void);
uint32_t OSGetProgressiveMode(void);
void* OSAlloc(uint32_t size);
void OSInitFastCast(void);
void OSPanic(const char* file, int line, const char* fmt, ...);
void OSReport(const char* fmt, ...);

// DVD
void DVDInit(void);

// VI
void VIInit(void);
int VIGetDTVStatus(void);
void VIConfigure(const void* rmode);
void VIConfigurePan(uint16_t xOrg, uint16_t yOrg, uint16_t width, uint16_t height);
uint32_t VIGetTvFormat(void);

// PAD
void PADInit(void);

// GX
void GXInit(void* base, uint32_t size);
void GXAdjustForOverscan(void* rmode, void* out, uint16_t hor, uint16_t ver);
void GXSetViewport(float xOrig, float yOrig, float wd, float ht, float nearZ, float farZ);
void GXSetScissor(uint32_t xOrigin, uint32_t yOrigin, uint32_t wd, uint32_t ht);
void GXSetDispCopySrc(uint16_t left, uint16_t top, uint16_t wd, uint16_t ht);
void GXSetDispCopyDst(uint16_t wd, uint16_t ht);
float GXSetDispCopyYScale(float yscale);

static uint8_t dummy_rmode[256] __attribute__((aligned(32)));

typedef struct {
  uint32_t viTVmode;
  uint16_t fbWidth;
  uint16_t efbHeight;
  uint16_t xfbHeight;
  uint16_t viXOrigin;
  uint16_t viYOrigin;
  uint16_t viWidth;
  uint16_t viHeight;
  uint32_t xFBmode;
} GXRenderModeObj;

static void init_dummy_rmode(void) {
  // Provide deterministic fields so VIConfigure/GXAdjustForOverscan mutate state.
  GXRenderModeObj *rm = (GXRenderModeObj*)dummy_rmode;
  memset(rm, 0, sizeof(*rm));
  rm->viTVmode = 0;
  rm->fbWidth = 640;
  rm->efbHeight = 480;
  rm->xfbHeight = 480;
  rm->viXOrigin = 0;
  rm->viYOrigin = 0;
  rm->viWidth = 640;
  rm->viHeight = 480;
  rm->xFBmode = 1;
}

static void run_chain(void) {
  OSInit();
  DVDInit();
  VIInit();
  PADInit();

  (void)OSGetProgressiveMode();
  (void)VIGetDTVStatus();

  VIConfigure(dummy_rmode);
  VIConfigurePan(0, 0, 0, 0);

  (void)OSAlloc(0x100);
  GXInit((void*)0x81000000u, 0x10000);
  OSInitFastCast();
  (void)VIGetTvFormat();

  uint8_t overscan_out[256] __attribute__((aligned(32)));
  GXAdjustForOverscan(dummy_rmode, overscan_out, 0, 0);
  GXSetViewport(0.0f, 0.0f, 640.0f, 480.0f, 0.0f, 1.0f);
  GXSetScissor(0, 0, 640, 480);
  GXSetDispCopySrc(0, 0, 640, 480);
  GXSetDispCopyDst(640, 480);
  (void)GXSetDispCopyYScale(1.0f);

  OSReport("mp4_init_chain_001 done\n");
}

// Mirror the DOL snapshot layout so we can compare a small deterministic window
// inside the full MEM1 dump.
static void emit_snapshot(GcRam *ram) {
  uint8_t *out = gc_ram_ptr(ram, SNAPSHOT_ADDR, 0x4C);
  if (!out) return;

  // Header
  store_u32be(out + 0x00, 0x534D4F4Bu); // "SMOK"
  store_u32be(out + 0x04, 0x00000001u); // version
  store_u32be(out + 0x08, (uint32_t)(uintptr_t)OSGetArenaLo());
  store_u32be(out + 0x0C, (uint32_t)(uintptr_t)OSGetArenaHi());

  // DVD
  extern uint32_t gc_dvd_initialized;
  extern uint32_t gc_dvd_drive_status;
  store_u32be(out + 0x10, gc_dvd_initialized);
  store_u32be(out + 0x14, gc_dvd_drive_status);

  // VI
  extern uint32_t gc_vi_change_mode;
  extern uint32_t gc_vi_disp_pos_x;
  extern uint32_t gc_vi_disp_pos_y;
  extern uint32_t gc_vi_disp_size_x;
  extern uint32_t gc_vi_disp_size_y;
  extern uint32_t gc_vi_fb_size_x;
  extern uint32_t gc_vi_fb_size_y;
  extern uint32_t gc_vi_xfb_mode;
  extern uint32_t gc_vi_helper_calls;
  store_u32be(out + 0x18, gc_vi_change_mode);
  store_u32be(out + 0x1C, gc_vi_disp_pos_x);
  store_u32be(out + 0x20, gc_vi_disp_pos_y);
  store_u32be(out + 0x24, gc_vi_disp_size_x);
  store_u32be(out + 0x28, gc_vi_disp_size_y);
  store_u32be(out + 0x2C, gc_vi_fb_size_x);
  store_u32be(out + 0x30, gc_vi_fb_size_y);
  store_u32be(out + 0x34, gc_vi_xfb_mode);
  store_u32be(out + 0x38, gc_vi_helper_calls);

  // PAD
  extern uint32_t gc_pad_initialized;
  store_u32be(out + 0x3C, gc_pad_initialized);

  // GX (subset)
  extern uint32_t gc_gx_cp_disp_src;
  extern uint32_t gc_gx_cp_disp_size;
  extern uint32_t gc_gx_copy_disp_dest;
  store_u32be(out + 0x40, gc_gx_cp_disp_src);
  store_u32be(out + 0x44, gc_gx_cp_disp_size);
  store_u32be(out + 0x48, gc_gx_copy_disp_dest);
}

// Minimal DOL loader for host-side "actual" RAM dump comparability.
// We load the DOL segments into the same addresses Dolphin uses, then run sdk_port
// against the same virtual RAM buffer. This makes MEM1 dumps comparable even though
// host isn't executing PPC code.
typedef struct {
  uint32_t text_off[7];
  uint32_t data_off[11];
  uint32_t text_addr[7];
  uint32_t data_addr[11];
  uint32_t text_size[7];
  uint32_t data_size[11];
  uint32_t bss_addr;
  uint32_t bss_size;
  uint32_t entry;
} DolHeader;

static uint32_t be32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static int load_dol_into_ram(GcRam *ram, const char *dol_path) {
  FILE *f = fopen(dol_path, "rb");
  if (!f) {
    fprintf(stderr, "[host] failed to open DOL: %s\n", dol_path);
    return 1;
  }

  uint8_t hdr_raw[0x100];
  if (fread(hdr_raw, 1, sizeof(hdr_raw), f) != sizeof(hdr_raw)) {
    fprintf(stderr, "[host] failed to read DOL header: %s\n", dol_path);
    fclose(f);
    return 2;
  }

  DolHeader h;
  memset(&h, 0, sizeof(h));

  // Header layout (big-endian u32):
  // 0x00.. text_off[7]
  // 0x1C.. data_off[11]
  // 0x48.. text_addr[7]
  // 0x64.. data_addr[11]
  // 0x90.. text_size[7]
  // 0xAC.. data_size[11]
  // 0xD8 bss_addr, 0xDC bss_size, 0xE0 entry
  for (int i = 0; i < 7; i++) h.text_off[i] = be32(&hdr_raw[0x00 + i * 4]);
  for (int i = 0; i < 11; i++) h.data_off[i] = be32(&hdr_raw[0x1C + i * 4]);
  for (int i = 0; i < 7; i++) h.text_addr[i] = be32(&hdr_raw[0x48 + i * 4]);
  for (int i = 0; i < 11; i++) h.data_addr[i] = be32(&hdr_raw[0x64 + i * 4]);
  for (int i = 0; i < 7; i++) h.text_size[i] = be32(&hdr_raw[0x90 + i * 4]);
  for (int i = 0; i < 11; i++) h.data_size[i] = be32(&hdr_raw[0xAC + i * 4]);
  h.bss_addr = be32(&hdr_raw[0xD8]);
  h.bss_size = be32(&hdr_raw[0xDC]);
  h.entry = be32(&hdr_raw[0xE0]);

  // Load text/data segments.
  uint8_t *buf = (uint8_t *)malloc(4 * 1024 * 1024);
  if (!buf) {
    fclose(f);
    return 3;
  }

  for (int i = 0; i < 7; i++) {
    if (h.text_off[i] == 0 || h.text_size[i] == 0) continue;
    if (fseek(f, (long)h.text_off[i], SEEK_SET) != 0) continue;
    uint32_t sz = h.text_size[i];
    if (sz > 4 * 1024 * 1024) {
      fprintf(stderr, "[host] text segment too big (%u)\n", (unsigned)sz);
      free(buf);
      fclose(f);
      return 4;
    }
    if (fread(buf, 1, sz, f) != sz) {
      fprintf(stderr, "[host] failed reading text seg %d\n", i);
      free(buf);
      fclose(f);
      return 5;
    }
    uint8_t *dst = gc_ram_ptr(ram, h.text_addr[i], sz);
    if (dst) memcpy(dst, buf, sz);
  }

  for (int i = 0; i < 11; i++) {
    if (h.data_off[i] == 0 || h.data_size[i] == 0) continue;
    if (fseek(f, (long)h.data_off[i], SEEK_SET) != 0) continue;
    uint32_t sz = h.data_size[i];
    if (sz > 4 * 1024 * 1024) {
      fprintf(stderr, "[host] data segment too big (%u)\n", (unsigned)sz);
      free(buf);
      fclose(f);
      return 6;
    }
    if (fread(buf, 1, sz, f) != sz) {
      fprintf(stderr, "[host] failed reading data seg %d\n", i);
      free(buf);
      fclose(f);
      return 7;
    }
    uint8_t *dst = gc_ram_ptr(ram, h.data_addr[i], sz);
    if (dst) memcpy(dst, buf, sz);
  }

  // Zero BSS.
  if (h.bss_addr && h.bss_size) {
    uint8_t *dst = gc_ram_ptr(ram, h.bss_addr, h.bss_size);
    if (dst) memset(dst, 0, h.bss_size);
  }

  free(buf);
  fclose(f);
  return 0;
}

int main(void) {
  GcRam ram;
  if (gc_ram_init(&ram, 0x80000000u, 0x01800000u) != 0) return 2;

  // Start from a DOL-loaded state for comparability with Dolphin dumps.
  const char *repo_root = getenv("GC_REPO_ROOT");
  char dol_path[1024];
  if (repo_root && repo_root[0]) {
    snprintf(dol_path, sizeof(dol_path),
             "%s/tests/sdk/smoke/mp4_init_chain_001/dol/mp4/mp4_init_chain_001/mp4_init_chain_001.dol",
             repo_root);
  } else {
    // Fallback: relative to cwd.
    snprintf(dol_path, sizeof(dol_path),
             "tests/sdk/smoke/mp4_init_chain_001/dol/mp4/mp4_init_chain_001/mp4_init_chain_001.dol");
  }

  if (load_dol_into_ram(&ram, dol_path) != 0) {
    gc_ram_free(&ram);
    return 4;
  }

  // Provide address translation for sdk_port.
  gc_mem_set(ram.base, ram.size, ram.buf);
  gc_sdk_state_reset();

  init_dummy_rmode();
  run_chain();

  // Marker in big-endian to match Dolphin RAM dumps.
  uint8_t *p = gc_ram_ptr(&ram, 0x80300000u, 8);
  store_u32be(p + 0, 0xC0DEF00Du);
  store_u32be(p + 4, 0x00000014u);

  emit_snapshot(&ram);

  // Dump full MEM1.
  if (gc_ram_dump(&ram, 0x80000000u, 0x01800000u,
                  "../actual/mp4_init_chain_001_mem1.bin") != 0) {
    gc_ram_free(&ram);
    return 3;
  }

  gc_ram_free(&ram);
  return 0;
}
