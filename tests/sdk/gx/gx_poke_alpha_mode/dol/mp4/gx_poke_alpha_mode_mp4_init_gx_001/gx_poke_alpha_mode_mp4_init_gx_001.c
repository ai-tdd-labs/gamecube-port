#include <stdint.h>

typedef uint32_t u32;
static volatile u32 s_func;
static volatile u32 s_thresh;
static void GXPokeAlphaMode(u32 func, u32 thresh) { s_func = func; s_thresh = thresh; }
int main(void) {
  GXPokeAlphaMode(7, 0);
  *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
  *(volatile u32 *)0x80300004 = s_func;
  *(volatile u32 *)0x80300008 = s_thresh;
  while (1) {}
}
