#include <stdint.h>
typedef uint32_t u32;

static volatile u32 s_cnt;
static u32 VIGetRetraceCount(void) { return s_cnt; }

int main(void) {
    s_cnt = 123;
    u32 v = VIGetRetraceCount();

    *(volatile u32*)0x80300000 = 0xDEADBEEFu;
    *(volatile u32*)0x80300004 = v;
    while (1) {}
}
