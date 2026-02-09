#include <stdint.h>

#include "gc_host_ram.h"
#include "gc_host_scenario.h"

#include "gc_mem.h"
#include "sdk_state.h"

void GXPosition3s16(int16_t x, int16_t y, int16_t z);
void GXColor1x8(uint8_t idx);
void GXTexCoord2f32(float s, float t);
void GXPosition2f32(float x, float y);
void GXColor3u8(uint8_t r, uint8_t g, uint8_t b);

static inline void store_u32be(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)(v >> 0);
}

enum { SNAPSHOT_ADDR = 0x80300100u };

const char *gc_scenario_label(void) { return "smoke/mp4_pfdrawfonts_chain_001"; }

const char *gc_scenario_out_path(void) {
  return "tests/sdk/smoke/mp4_pfdrawfonts_chain_001/actual/mp4_pfdrawfonts_chain_001_marker.bin";
}

static void emit_snapshot(GcRam *ram) {
  uint8_t *out = gc_ram_ptr(ram, SNAPSHOT_ADDR, 0x40);
  if (!out) return;

  store_u32be(out + 0x00, 0x534D4F4Bu); // "SMOK"
  store_u32be(out + 0x04, 0x00000001u); // version

  extern uint32_t gc_gx_pos3s16_x;
  extern uint32_t gc_gx_pos3s16_y;
  extern uint32_t gc_gx_pos3s16_z;
  extern uint32_t gc_gx_pos2f32_x_bits;
  extern uint32_t gc_gx_pos2f32_y_bits;
  extern uint32_t gc_gx_texcoord2f32_s_bits;
  extern uint32_t gc_gx_texcoord2f32_t_bits;
  extern uint32_t gc_gx_color1x8_last;
  extern uint32_t gc_gx_color3u8_last;

  store_u32be(out + 0x08, gc_gx_pos3s16_x);
  store_u32be(out + 0x0C, gc_gx_pos3s16_y);
  store_u32be(out + 0x10, gc_gx_pos3s16_z);

  store_u32be(out + 0x14, gc_gx_pos2f32_x_bits);
  store_u32be(out + 0x18, gc_gx_pos2f32_y_bits);
  store_u32be(out + 0x1C, gc_gx_texcoord2f32_s_bits);
  store_u32be(out + 0x20, gc_gx_texcoord2f32_t_bits);

  store_u32be(out + 0x24, gc_gx_color1x8_last);
  store_u32be(out + 0x28, gc_gx_color3u8_last);
}

void gc_scenario_run(GcRam *ram) {
  // Ensure MEM1-backed sdk_state exists (even though this chain mostly uses GX mirrors).
  gc_mem_set(0x80000000u, 0x01800000u, ram->buf);
  gc_sdk_state_reset();

  // pfDrawFonts quad path snippet (representative values).
  // x=100, y=50, z=0, color index=15, texcoords = (4/16, 2/16).
  GXPosition3s16(100, 50, 0);
  GXColor1x8(15);
  GXTexCoord2f32(4.0f / 16.0f, 2.0f / 16.0f);

  GXPosition3s16(108, 50, 0);
  GXColor1x8(15);
  GXTexCoord2f32(5.0f / 16.0f, 2.0f / 16.0f);

  // WireDraw-like snippet.
  GXPosition2f32(32.0f, 24.0f);
  GXColor3u8(255, 0, 0);

  uint8_t *marker = gc_ram_ptr(ram, 0x80300000u, 8);
  if (marker) {
    store_u32be(marker + 0x00, 0xC0DEF00Du);
    store_u32be(marker + 0x04, 0x00000005u);
  }
  emit_snapshot(ram);
}
