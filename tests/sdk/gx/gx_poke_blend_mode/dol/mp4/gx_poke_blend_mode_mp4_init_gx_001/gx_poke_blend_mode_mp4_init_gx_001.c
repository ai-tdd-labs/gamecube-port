#include <stdint.h>

typedef uint32_t u32;
static volatile u32 s_type;
static volatile u32 s_src;
static volatile u32 s_dst;
static volatile u32 s_op;
static void GXPokeBlendMode(u32 type, u32 src, u32 dst, u32 op) { s_type = type; s_src = src; s_dst = dst; s_op = op; }
int main(void) {
  GXPokeBlendMode(0, 0, 1, 15);
  *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
  *(volatile u32 *)0x80300004 = s_type;
  *(volatile u32 *)0x80300008 = s_src;
  *(volatile u32 *)0x8030000C = s_dst;
  *(volatile u32 *)0x80300010 = s_op;
  while (1) {}
}
