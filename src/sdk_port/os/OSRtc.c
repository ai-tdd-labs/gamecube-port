#include <stdint.h>

// Minimal SRAM + OSGetProgressiveMode port for host scenarios.

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int64_t s64;
typedef int BOOL;
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

u8 gc_sram_flags;
u32 gc_sram_unlock_calls;
u64 gc_os_tick_counter;

typedef struct OSSram {
    u8 flags;
} OSSram;

static OSSram s_sram;

OSSram *__OSLockSramHACK(void) {
    s_sram.flags = gc_sram_flags;
    return &s_sram;
}

void __OSUnlockSram(BOOL commit) {
    (void)commit;
    gc_sram_flags = s_sram.flags;
    gc_sram_unlock_calls++;
}

u32 OSGetProgressiveMode(void) {
    OSSram *sram;
    u32 mode;

    sram = __OSLockSramHACK();
    mode = (sram->flags & 0x80u) >> 7;
    __OSUnlockSram(FALSE);
    return mode;
}

#define OS_TIME_MONTH_MAX 12
#define OS_TIME_WEEK_DAY_MAX 7
#define OS_TIME_YEAR_DAY_MAX 365
#define OS_TIMER_CLOCK (162000000u / 4u)

typedef struct OSCalendarTime {
    int32_t sec;
    int32_t min;
    int32_t hour;
    int32_t mday;
    int32_t mon;
    int32_t year;
    int32_t wday;
    int32_t yday;
    int32_t msec;
    int32_t usec;
} OSCalendarTime;

static int32_t YearDays[OS_TIME_MONTH_MAX] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
static int32_t LeapYearDays[OS_TIME_MONTH_MAX] = {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335};

static int IsLeapYear(int32_t year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static int32_t GetLeapDays(int32_t year) {
    if (year < 1) {
        return 0;
    }
    return (year + 3) / 4 - (year - 1) / 100 + (year - 1) / 400;
}

static void GetDates(int32_t days, OSCalendarTime *cal) {
    int32_t year;
    int32_t totalDays;
    int32_t *p_days;
    int32_t month;

    cal->wday = (days + 6) % OS_TIME_WEEK_DAY_MAX;
    for (year = days / OS_TIME_YEAR_DAY_MAX; days < (totalDays = year * OS_TIME_YEAR_DAY_MAX + GetLeapDays(year));) {
        year--;
    }

    days -= totalDays;
    cal->year = year;
    cal->yday = days;

    p_days = IsLeapYear(year) ? LeapYearDays : YearDays;
    month = OS_TIME_MONTH_MAX;
    while (days < p_days[--month]) {
        ;
    }
    cal->mon = month;
    cal->mday = days - p_days[month] + 1;
}

#define OSSecondsToTicks(sec) ((s64)(sec) * (s64)OS_TIMER_CLOCK)
#define OSTicksToSeconds(ticks) ((ticks) / (s64)OS_TIMER_CLOCK)
#define OSTicksToMilliseconds(ticks) ((ticks) / ((s64)OS_TIMER_CLOCK / 1000))
#define OSTicksToMicroseconds(ticks) (((ticks) * 8) / ((s64)OS_TIMER_CLOCK / 125000))
#define BIAS (2000 * 365 + (2000 + 3) / 4 - (2000 - 1) / 100 + (2000 - 1) / 400)

u32 OSGetTick(void) {
    // Deterministic host monotonic clock: advance by 1ms worth of ticks/call.
    gc_os_tick_counter += ((u64)OS_TIMER_CLOCK / 1000u);
    return (u32)gc_os_tick_counter;
}

void OSTicksToCalendarTime(s64 ticks, OSCalendarTime *td) {
    int32_t days;
    int32_t secs;
    s64 d;

    d = ticks % OSSecondsToTicks(1);
    if (d < 0) {
        d += OSSecondsToTicks(1);
    }
    td->usec = (int32_t)(OSTicksToMicroseconds(d) % 1000);
    td->msec = (int32_t)(OSTicksToMilliseconds(d) % 1000);

    ticks -= d;
    days = (int32_t)(OSTicksToSeconds(ticks) / 86400 + BIAS);
    secs = (int32_t)(OSTicksToSeconds(ticks) % 86400);
    if (secs < 0) {
        days -= 1;
        secs += 24 * 60 * 60;
    }

    GetDates(days, td);
    td->hour = secs / 60 / 60;
    td->min = (secs / 60) % 60;
    td->sec = secs % 60;
}
