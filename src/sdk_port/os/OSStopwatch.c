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

void OSInitStopwatch(OSStopwatch *sw, char *name) {
    sw->name = name;
    sw->total = 0;
    sw->hits = 0;
    sw->min = 0x00000000FFFFFFFFull;
    sw->max = 0;
}

void OSResetStopwatch(OSStopwatch *sw) {
    OSInitStopwatch(sw, sw->name);
}
