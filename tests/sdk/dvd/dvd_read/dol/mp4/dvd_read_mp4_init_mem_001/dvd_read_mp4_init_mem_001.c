#include <stdint.h>
typedef uint32_t u32;

typedef struct { u32 _dummy; } DVDFileInfo;

static volatile u32 s_calls;
static volatile u32 s_last_len;
static volatile u32 s_last_off;
static int DVDRead(DVDFileInfo *fi, void *addr, int len, int off) {
    (void)fi; (void)addr;
    s_calls++;
    s_last_len = (u32)len;
    s_last_off = (u32)off;
    return len;
}

int main(void) {
    DVDFileInfo fi;
    int r = DVDRead(&fi, (void*)0x81230000u, 0x200, 0x10);

    *(volatile u32*)0x80300000 = 0xDEADBEEFu;
    *(volatile u32*)0x80300004 = (u32)r;
    *(volatile u32*)0x80300008 = s_calls;
    *(volatile u32*)0x8030000C = s_last_len;
    *(volatile u32*)0x80300010 = s_last_off;
    while (1) {}
}
