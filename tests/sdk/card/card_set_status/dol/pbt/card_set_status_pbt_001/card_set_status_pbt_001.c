#include <stdint.h>
#include "src/sdk_port/gc_mem.c"

typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t s32;

void oracle_card_set_status_pbt_001_suite(u8* out);

static inline void wr32be(u8* p, u32 v) {
  p[0] = (u8)(v >> 24);
  p[1] = (u8)(v >> 16);
  p[2] = (u8)(v >> 8);
  p[3] = (u8)v;
}

int main(void) {
  gc_mem_set(0x80000000u, 0x01800000u, (void*)0x80000000u);

  volatile u8* out = (volatile u8*)0x80300000u;
  for (u32 i = 0; i < 0x400u; i++) {
    out[i] = 0;
  }

  oracle_card_set_status_pbt_001_suite((u8*)out);

  while (1) {
    __asm__ volatile("nop");
  }

  return (s32)sizeof(u32);
}
