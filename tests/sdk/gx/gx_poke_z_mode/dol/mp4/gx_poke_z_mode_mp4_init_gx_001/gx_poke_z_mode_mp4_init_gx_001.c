#include <stdint.h>

typedef uint32_t u32;
static volatile u32 s_enable;
static volatile u32 s_func;
static volatile u32 s_update;
static void GXPokeZMode(u32 enable, u32 func, u32 update) { s_enable = enable; s_func = func; s_update = update; }
int main(void) {
  GXPokeZMode(1, 7, 1);
  *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
  *(volatile u32 *)0x80300004 = s_enable;
  *(volatile u32 *)0x80300008 = s_func;
  *(volatile u32 *)0x8030000C = s_update;
  while (1) {}
}
