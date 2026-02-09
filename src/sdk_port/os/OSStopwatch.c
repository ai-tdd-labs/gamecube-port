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

// Deterministic OSGetTime() for host scenarios.
// We intentionally do not model TB frequency here; this is sufficient for
// MP4 perf bookkeeping and for bit-exact deterministic tests.
static u64 gc_os_time_counter;
static u64 OSGetTime(void) {
    return ++gc_os_time_counter;
}

void OSInitStopwatch(OSStopwatch *sw, char *name) {
    sw->name = name;
    sw->total = 0;
    sw->hits = 0;
    sw->min = 0x00000000FFFFFFFFull;
    sw->max = 0;
    sw->running = 0;
    sw->last = 0;
}

void OSStartStopwatch(OSStopwatch *sw) {
    sw->running = 1;
    sw->last = OSGetTime();
}

void OSStopStopwatch(OSStopwatch *sw) {
    long long interval;
    if (sw->running != 0) {
        interval = (long long)(OSGetTime() - sw->last);
        sw->total += (u64)interval;
        sw->running = 0;
        sw->hits++;
        if (sw->max < (u64)interval) {
            sw->max = (u64)interval;
        }
        if ((u64)interval < sw->min) {
            sw->min = (u64)interval;
        }
    }
}

long long OSCheckStopwatch(OSStopwatch *sw) {
    long long currTotal;
    currTotal = (long long)sw->total;
    if (sw->running != 0) {
        currTotal += (long long)(OSGetTime() - sw->last);
    }
    return currTotal;
}

void OSResetStopwatch(OSStopwatch *sw) {
    OSInitStopwatch(sw, sw->name);
}
