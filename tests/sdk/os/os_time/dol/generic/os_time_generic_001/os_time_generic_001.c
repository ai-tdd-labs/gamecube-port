#include <stdint.h>

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

static inline void wr32be(volatile uint8_t *p, u32 v) {
    p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16);
    p[2]=(uint8_t)(v>>8);  p[3]=(uint8_t)(v);
}

/* Dump entire OSCalendarTime (10 s32 fields = 40 bytes) */
static inline void dump_cal(volatile uint8_t *p, const OSCalendarTime *t) {
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

#define TICKS_PER_SEC (162000000LL / 4LL)  /* 40500000 */

int main(void) {
    volatile uint8_t *out = (volatile uint8_t *)0x80300000;
    for (u32 i = 0; i < 0x180; i++) out[i] = 0;

    u32 off = 0;
    OSCalendarTime cal;

    /* --- T1: ticks=0 (GC epoch: 2000-01-01 00:00:00.000) --- */
    OSTicksToCalendarTime(0, &cal);
    wr32be(out + off, 0xCA1E0001u); off += 4;
    dump_cal(out + off, &cal); off += 40;
    /* off = 44 */

    /* --- T2: exactly 1 second --- */
    OSTicksToCalendarTime(TICKS_PER_SEC, &cal);
    wr32be(out + off, 0xCA1E0002u); off += 4;
    dump_cal(out + off, &cal); off += 40;
    /* off = 88 */

    /* --- T3: 1 day (86400 seconds) --- */
    OSTicksToCalendarTime(86400LL * TICKS_PER_SEC, &cal);
    wr32be(out + off, 0xCA1E0003u); off += 4;
    dump_cal(out + off, &cal); off += 40;
    /* off = 132 */

    /* --- T4: Feb 29, 2000 (leap year, day 59) --- */
    OSTicksToCalendarTime(59LL * 86400LL * TICKS_PER_SEC, &cal);
    wr32be(out + off, 0xCA1E0004u); off += 4;
    dump_cal(out + off, &cal); off += 40;
    /* off = 176 */

    /* --- T5: 2001-01-01 (366 days from epoch, past first leap year) --- */
    OSTicksToCalendarTime(366LL * 86400LL * TICKS_PER_SEC, &cal);
    wr32be(out + off, 0xCA1E0005u); off += 4;
    dump_cal(out + off, &cal); off += 40;
    /* off = 220 */

    /* --- T6: half second with sub-second components --- */
    OSTicksToCalendarTime(TICKS_PER_SEC / 2, &cal);
    wr32be(out + off, 0xCA1E0006u); off += 4;
    dump_cal(out + off, &cal); off += 40;
    /* off = 264 */

    /* --- T7: large value: ~5 years (2005-01-01 approx) --- */
    /* 1826 days = 365*5 + 1 (one leap year 2004) */
    OSTicksToCalendarTime(1826LL * 86400LL * TICKS_PER_SEC, &cal);
    wr32be(out + off, 0xCA1E0007u); off += 4;
    dump_cal(out + off, &cal); off += 40;
    /* off = 308 */

    /* --- T8: specific time: 15:30:45.123 on day 100 --- */
    {
        s64 t = 100LL * 86400LL * TICKS_PER_SEC
              + 15LL * 3600LL * TICKS_PER_SEC
              + 30LL * 60LL * TICKS_PER_SEC
              + 45LL * TICKS_PER_SEC
              + 123LL * (TICKS_PER_SEC / 1000LL);
        OSTicksToCalendarTime(t, &cal);
        wr32be(out + off, 0xCA1E0008u); off += 4;
        dump_cal(out + off, &cal); off += 40;
    }
    /* off = 352 = 0x160 */

    while (1) { __asm__ volatile("nop"); }
    return 0;
}
