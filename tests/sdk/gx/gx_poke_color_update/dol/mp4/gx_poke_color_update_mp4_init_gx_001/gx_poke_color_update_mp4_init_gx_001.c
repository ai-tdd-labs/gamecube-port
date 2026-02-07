#include <stdint.h>

typedef uint32_t u32;
static volatile u32 s_enable;
static void GXPokeColorUpdate(u32 enable) { s_enable = enable; }
int main(void) {
  GXPokeColorUpdate(1);
  *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
  *(volatile u32 *)0x80300004 = s_enable;
  while (1) {}
}
