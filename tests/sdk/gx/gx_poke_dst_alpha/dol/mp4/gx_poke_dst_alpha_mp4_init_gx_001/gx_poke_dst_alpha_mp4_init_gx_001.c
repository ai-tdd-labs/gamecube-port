#include <stdint.h>

typedef uint32_t u32;
static volatile u32 s_enable;
static volatile u32 s_alpha;
static void GXPokeDstAlpha(u32 enable, u32 alpha) { s_enable = enable; s_alpha = alpha; }
int main(void) {
  GXPokeDstAlpha(0, 0);
  *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
  *(volatile u32 *)0x80300004 = s_enable;
  *(volatile u32 *)0x80300008 = s_alpha;
  while (1) {}
}
