#include <stdint.h>

// This smoke chain runs our sdk_port implementation on real PPC (in Dolphin).
// Oracle for phase A: "PPC build of sdk_port" vs "host build of sdk_port".
//
// This focuses on GX immediate-mode helpers used by MP4's pfDrawFonts().
// Those helpers write to the GX FIFO on real hardware, so our sdk_port provides
// deterministic "mirror last written values" observability.

#include "src/sdk_port/sdk_state.h"

void GXPosition3s16(int16_t x, int16_t y, int16_t z);
void GXColor1x8(uint8_t idx);
void GXTexCoord2f32(float s, float t);
void GXPosition2f32(float x, float y);
void GXColor3u8(uint8_t r, uint8_t g, uint8_t b);

enum { SNAPSHOT_ADDR = 0x80300100u };

static inline void gc_disable_interrupts(void) {
  uint32_t msr;
  __asm__ volatile("mfmsr %0" : "=r"(msr));
  msr &= ~0x8000u; // MSR[EE]
  __asm__ volatile("mtmsr %0; isync" : : "r"(msr));
}

static inline void gc_arm_decrementer_far_future(void) {
  __asm__ volatile(
      "lis 3,0x7fff\n"
      "ori 3,3,0xffff\n"
      "mtdec 3\n"
      :
      :
      : "r3");
}

static inline void gc_safepoint(void) {
  gc_disable_interrupts();
  gc_arm_decrementer_far_future();
}

static inline void store_u32be_ptr(volatile uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)(v >> 0);
}

static void emit_snapshot(void) {
  volatile uint8_t *out = (volatile uint8_t *)SNAPSHOT_ADDR;

  store_u32be_ptr(out + 0x00, 0x534D4F4Bu); // "SMOK"
  store_u32be_ptr(out + 0x04, 0x00000001u); // version

  extern uint32_t gc_gx_pos3s16_x;
  extern uint32_t gc_gx_pos3s16_y;
  extern uint32_t gc_gx_pos3s16_z;
  extern uint32_t gc_gx_pos2f32_x_bits;
  extern uint32_t gc_gx_pos2f32_y_bits;
  extern uint32_t gc_gx_texcoord2f32_s_bits;
  extern uint32_t gc_gx_texcoord2f32_t_bits;
  extern uint32_t gc_gx_color1x8_last;
  extern uint32_t gc_gx_color3u8_last;

  // Position3s16 mirrors are stored as sign-extended u32 in sdk_port.
  store_u32be_ptr(out + 0x08, gc_gx_pos3s16_x);
  store_u32be_ptr(out + 0x0C, gc_gx_pos3s16_y);
  store_u32be_ptr(out + 0x10, gc_gx_pos3s16_z);

  // Position2f32 / TexCoord2f32 (raw float bits)
  store_u32be_ptr(out + 0x14, gc_gx_pos2f32_x_bits);
  store_u32be_ptr(out + 0x18, gc_gx_pos2f32_y_bits);
  store_u32be_ptr(out + 0x1C, gc_gx_texcoord2f32_s_bits);
  store_u32be_ptr(out + 0x20, gc_gx_texcoord2f32_t_bits);

  // Color mirrors
  store_u32be_ptr(out + 0x24, gc_gx_color1x8_last);
  store_u32be_ptr(out + 0x28, gc_gx_color3u8_last);
}

int main(void) {
  gc_safepoint();

  // Ensure MEM1-backed sdk_state exists (even though this chain mostly uses GX mirrors).
  gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);
  gc_sdk_state_reset();

  // pfDrawFonts quad path snippet (representative values).
  // x=100, y=50, z=0, color index=15, texcoords = (4/16, 2/16).
  GXPosition3s16(100, 50, 0);
  GXColor1x8(15);
  GXTexCoord2f32(4.0f / 16.0f, 2.0f / 16.0f);
  gc_safepoint();

  GXPosition3s16(108, 50, 0);
  GXColor1x8(15);
  GXTexCoord2f32(5.0f / 16.0f, 2.0f / 16.0f);
  gc_safepoint();

  // WireDraw-like snippet.
  GXPosition2f32(32.0f, 24.0f);
  GXColor3u8(255, 0, 0);
  gc_safepoint();

  *(volatile uint32_t *)0x80300000 = 0xC0DEF00Du;
  *(volatile uint32_t *)0x80300004 = 0x00000005u; // steps (rough)
  emit_snapshot();

  while (1) {
    gc_safepoint();
    __asm__ volatile("nop");
  }
  return 0;
}
