#include <stdint.h>

typedef uint32_t u32;
typedef uint64_t u64;

typedef struct OSStopwatch {
    char *name;
    u64 total;
    u32 hits;
    u32 running;
    u64 last;
    u64 min;
    u64 max;
} OSStopwatch;

static void OSInitStopwatch(OSStopwatch *sw, char *name) {
    sw->name = name;
    sw->total = 0;
    sw->hits = 0;
    sw->min = 0x00000000FFFFFFFFull;
    sw->max = 0;
}

int main(void) {
    OSStopwatch sw;

    sw.name = (char*)0;
    sw.total = 0x1111111122222222ull;
    sw.hits = 0x33333333u;
    sw.running = 0x44444444u;
    sw.last = 0x5555555566666666ull;
    sw.min = 0x7777777788888888ull;
    sw.max = 0x99999999AAAAAAAAull;

    OSInitStopwatch(&sw, (char*)"CPU");

    *(volatile u32*)0x80300000 = 0xDEADBEEFu;
    // Pointer values are not comparable across environments; omit by writing 0.
    *(volatile u32*)0x80300004 = 0;
    *(volatile u32*)0x80300008 = (u32)(sw.total >> 32);
    *(volatile u32*)0x8030000C = (u32)(sw.total & 0xFFFFFFFFu);
    *(volatile u32*)0x80300010 = sw.hits;
    *(volatile u32*)0x80300014 = (u32)(sw.min >> 32);
    *(volatile u32*)0x80300018 = (u32)(sw.min & 0xFFFFFFFFu);
    *(volatile u32*)0x8030001C = (u32)(sw.max >> 32);
    *(volatile u32*)0x80300020 = (u32)(sw.max & 0xFFFFFFFFu);

    // Verify untouched fields remain sentinel.
    *(volatile u32*)0x80300024 = sw.running;
    *(volatile u32*)0x80300028 = (u32)(sw.last >> 32);
    *(volatile u32*)0x8030002C = (u32)(sw.last & 0xFFFFFFFFu);

    while (1) {}
}
