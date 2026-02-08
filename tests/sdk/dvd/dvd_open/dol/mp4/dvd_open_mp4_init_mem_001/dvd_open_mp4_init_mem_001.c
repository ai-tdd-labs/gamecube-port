#include <stdint.h>
typedef uint32_t u32;

typedef struct { u32 _dummy; } DVDFileInfo;

static volatile u32 s_calls;
// MP4 init.c handles "missing /meminfo.bin" as a normal path.
static int DVDOpen(const char *path, DVDFileInfo *fi) { (void)fi; (void)path; s_calls++; return 0; }

int main(void) {
    DVDFileInfo fi;
    int ok = DVDOpen("/meminfo.bin", &fi);

    *(volatile u32*)0x80300000 = 0xDEADBEEFu;
    *(volatile u32*)0x80300004 = (u32)ok;
    *(volatile u32*)0x80300008 = s_calls;
    while (1) {}
}
