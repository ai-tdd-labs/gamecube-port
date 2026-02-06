#include <stdint.h>
typedef uint32_t u32;

static int OSReport(const char *msg, ...) {
    (void)msg;
    return 0;
}

int main(void) {
    int n = OSReport("hello %d", 123);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = (u32)n;
    while (1) {}
}
