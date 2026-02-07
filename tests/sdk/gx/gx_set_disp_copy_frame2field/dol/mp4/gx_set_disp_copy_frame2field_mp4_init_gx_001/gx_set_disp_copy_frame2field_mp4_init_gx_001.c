#include <stdint.h>

typedef uint32_t u32;
static volatile u32 s_mode;
static void GXSetDispCopyFrame2Field(u32 mode) { s_mode = mode; }
int main(void) {
  GXSetDispCopyFrame2Field(0);
  *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
  *(volatile u32 *)0x80300004 = s_mode;
  while (1) {}
}
