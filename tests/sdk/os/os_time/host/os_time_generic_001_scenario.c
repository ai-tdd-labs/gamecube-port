#include <stdint.h>
#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef int32_t  s32;
typedef int64_t  s64;

typedef struct OSCalendarTime {
    s32 sec;
    s32 min;
    s32 hour;
    s32 mday;
    s32 mon;
    s32 year;
    s32 wday;
    s32 yday;
    s32 msec;
    s32 usec;
} OSCalendarTime;

void OSTicksToCalendarTime(s64 ticks, OSCalendarTime *td);

/* OSReport needed by OSDumpStopwatch in OSRtc.c */
int OSReport(const char *msg, ...);

static inline void dump_cal(uint8_t *p, const OSCalendarTime *t) {
    wr32be(p + 0,  (u32)t->sec);
    wr32be(p + 4,  (u32)t->min);
    wr32be(p + 8,  (u32)t->hour);
    wr32be(p + 12, (u32)t->mday);
    wr32be(p + 16, (u32)t->mon);
    wr32be(p + 20, (u32)t->year);
    wr32be(p + 24, (u32)t->wday);
    wr32be(p + 28, (u32)t->yday);
    wr32be(p + 32, (u32)t->msec);
    wr32be(p + 36, (u32)t->usec);
}

#define TICKS_PER_SEC (162000000LL / 4LL)

const char *gc_scenario_label(void) { return "OS_TIME/generic"; }
const char *gc_scenario_out_path(void) { return "../actual/os_time_generic_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    (void)ram;

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x180);
    if (!p) die("gc_ram_ptr failed");
    for (u32 i = 0; i < 0x180; i += 4) wr32be(p + i, 0);

    u32 off = 0;
    OSCalendarTime cal;

    /* T1: epoch */
    OSTicksToCalendarTime(0, &cal);
    wr32be(p + off, 0xCA1E0001u); off += 4;
    dump_cal(p + off, &cal); off += 40;

    /* T2: 1 second */
    OSTicksToCalendarTime(TICKS_PER_SEC, &cal);
    wr32be(p + off, 0xCA1E0002u); off += 4;
    dump_cal(p + off, &cal); off += 40;

    /* T3: 1 day */
    OSTicksToCalendarTime(86400LL * TICKS_PER_SEC, &cal);
    wr32be(p + off, 0xCA1E0003u); off += 4;
    dump_cal(p + off, &cal); off += 40;

    /* T4: Feb 29 2000 (day 59) */
    OSTicksToCalendarTime(59LL * 86400LL * TICKS_PER_SEC, &cal);
    wr32be(p + off, 0xCA1E0004u); off += 4;
    dump_cal(p + off, &cal); off += 40;

    /* T5: 2001-01-01 (366 days) */
    OSTicksToCalendarTime(366LL * 86400LL * TICKS_PER_SEC, &cal);
    wr32be(p + off, 0xCA1E0005u); off += 4;
    dump_cal(p + off, &cal); off += 40;

    /* T6: half second */
    OSTicksToCalendarTime(TICKS_PER_SEC / 2, &cal);
    wr32be(p + off, 0xCA1E0006u); off += 4;
    dump_cal(p + off, &cal); off += 40;

    /* T7: ~5 years (2005-01-01) */
    OSTicksToCalendarTime(1826LL * 86400LL * TICKS_PER_SEC, &cal);
    wr32be(p + off, 0xCA1E0007u); off += 4;
    dump_cal(p + off, &cal); off += 40;

    /* T8: specific time on day 100 */
    {
        s64 t = 100LL * 86400LL * TICKS_PER_SEC
              + 15LL * 3600LL * TICKS_PER_SEC
              + 30LL * 60LL * TICKS_PER_SEC
              + 45LL * TICKS_PER_SEC
              + 123LL * (TICKS_PER_SEC / 1000LL);
        OSTicksToCalendarTime(t, &cal);
        wr32be(p + off, 0xCA1E0008u); off += 4;
        dump_cal(p + off, &cal); off += 40;
    }
}
