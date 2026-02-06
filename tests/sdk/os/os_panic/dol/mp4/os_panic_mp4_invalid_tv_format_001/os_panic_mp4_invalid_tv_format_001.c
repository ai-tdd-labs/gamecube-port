#include <stdint.h>
typedef uint32_t u32;

static volatile u32 s_panic_calls;

static void OSPanic(const char *file, int line, const char *msg, ...) {
    (void)file; (void)line; (void)msg;
    s_panic_calls++;
}

int main(void) {
    OSPanic("init.c", 167, "DEMOInit: invalid TV format\n");

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_panic_calls;
    while (1) {}
}
