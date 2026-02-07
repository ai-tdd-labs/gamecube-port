#include <stdint.h>

typedef uint32_t u32;
static volatile u32 s_field_mode;
static volatile u32 s_half_aspect;
static void GXSetFieldMode(u32 field_mode, u32 half_aspect) { s_field_mode = field_mode; s_half_aspect = half_aspect; }
int main(void) {
  GXSetFieldMode(0, 0);
  *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
  *(volatile u32 *)0x80300004 = s_field_mode;
  *(volatile u32 *)0x80300008 = s_half_aspect;
  while (1) {}
}
