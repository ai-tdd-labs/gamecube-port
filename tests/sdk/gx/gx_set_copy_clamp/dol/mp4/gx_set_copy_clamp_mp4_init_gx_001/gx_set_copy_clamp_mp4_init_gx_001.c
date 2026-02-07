#include <stdint.h>

typedef uint32_t u32;
static volatile u32 s_clamp;
static void GXSetCopyClamp(u32 clamp) { s_clamp = clamp; }
int main(void) {
  GXSetCopyClamp(3);
  *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
  *(volatile u32 *)0x80300004 = s_clamp;
  while (1) {}
}
