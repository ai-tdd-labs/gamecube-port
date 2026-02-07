#include <stdint.h>

typedef uint32_t u32;
static volatile u32 s_even;
static volatile u32 s_odd;
static void GXSetFieldMask(u32 even, u32 odd) { s_even = even; s_odd = odd; }
int main(void) {
  GXSetFieldMask(1, 1);
  *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
  *(volatile u32 *)0x80300004 = s_even;
  *(volatile u32 *)0x80300008 = s_odd;
  while (1) {}
}
