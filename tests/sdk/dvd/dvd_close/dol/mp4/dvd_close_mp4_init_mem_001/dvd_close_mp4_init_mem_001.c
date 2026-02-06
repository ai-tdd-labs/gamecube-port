#include <stdint.h>
typedef uint32_t u32;

typedef struct { u32 _dummy; } DVDFileInfo;

static volatile u32 s_calls;
static int DVDClose(DVDFileInfo *fi) { (void)fi; s_calls++; return 1; }

int main(void) {
    DVDFileInfo fi;
    int ok = DVDClose(&fi);

    *(volatile u32*)0x80300000 = 0xDEADBEEFu;
    *(volatile u32*)0x80300004 = (u32)ok;
    *(volatile u32*)0x80300008 = s_calls;
    while (1) {}
}
