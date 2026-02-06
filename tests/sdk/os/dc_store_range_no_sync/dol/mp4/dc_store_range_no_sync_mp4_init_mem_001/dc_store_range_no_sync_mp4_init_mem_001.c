#include <stdint.h>
typedef uint32_t u32;

static volatile u32 s_last_addr;
static volatile u32 s_last_len;

static void DCStoreRangeNoSync(void *addr, u32 nbytes) {
    s_last_addr = (u32)(uintptr_t)addr;
    s_last_len = nbytes;
}

int main(void) {
    DCStoreRangeNoSync((void*)0x81234560u, 0x1234u);

    *(volatile u32*)0x80300000 = 0xDEADBEEF;
    *(volatile u32*)0x80300004 = s_last_addr;
    *(volatile u32*)0x80300008 = s_last_len;
    while (1) {}
}
