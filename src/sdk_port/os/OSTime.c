/*
 * OSTime.c — Port of decomp_mario_party_4/src/dolphin/os/OSTime.c
 *
 * Pure host computation — no gc_mem or hardware interaction.
 * GC timer clock = bus_clock / 4 = 162,000,000 / 4 = 40,500,000 Hz.
 */

#include <stdint.h>

typedef int32_t  s32;
typedef int64_t  s64;

/* ── GC tick rate constants ── */
#define GC_BUS_CLOCK      162000000u
#define GC_TIMER_CLOCK    (GC_BUS_CLOCK / 4)  /* 40,500,000 */

#define TICKS_PER_SEC     ((s64)GC_TIMER_CLOCK)
#define TICKS_PER_MSEC    ((s64)(GC_TIMER_CLOCK / 1000))
#define TICKS_PER_USEC_NUM 8
#define TICKS_PER_USEC_DEN ((s64)(GC_TIMER_CLOCK / 125000))

#define OSSecondsToTicks(sec)          ((s64)(sec) * TICKS_PER_SEC)
#define OSTicksToSeconds(ticks)        ((ticks) / TICKS_PER_SEC)
#define OSTicksToMilliseconds(ticks)   ((ticks) / TICKS_PER_MSEC)
#define OSTicksToMicroseconds(ticks)   (((ticks) * TICKS_PER_USEC_NUM) / TICKS_PER_USEC_DEN)

/* ── Calendar time struct (matches SDK OSCalendarTime) ── */
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

/* ── Internal constants and tables ── */

#define OS_TIME_MONTH_MAX    12
#define OS_TIME_WEEK_DAY_MAX  7
#define OS_TIME_YEAR_DAY_MAX 365

static s32 YearDays[OS_TIME_MONTH_MAX] =
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
static s32 LeapYearDays[OS_TIME_MONTH_MAX] =
    { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 };

static int IsLeapYear(s32 year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static s32 GetLeapDays(s32 year)
{
    if (year < 1) return 0;
    return (year + 3) / 4 - (year - 1) / 100 + (year - 1) / 400;
}

static void GetDates(s32 days, OSCalendarTime *cal)
{
    s32 year;
    s32 totalDays;
    s32 *p_days;
    s32 month;

    cal->wday = (days + 6) % OS_TIME_WEEK_DAY_MAX;

    for (year = days / OS_TIME_YEAR_DAY_MAX;
         days < (totalDays = year * OS_TIME_YEAR_DAY_MAX + GetLeapDays(year));)
    {
        year--;
    }

    days -= totalDays;
    cal->year = year;
    cal->yday = days;

    p_days = IsLeapYear(year) ? LeapYearDays : YearDays;
    month = OS_TIME_MONTH_MAX;
    while (days < p_days[--month])
        ;
    cal->mon = month;
    cal->mday = days - p_days[month] + 1;
}

#define BIAS (2000 * 365 + (2000 + 3) / 4 - (2000 - 1) / 100 + (2000 - 1) / 400)

void OSTicksToCalendarTime(s64 ticks, OSCalendarTime *td)
{
    s32 days;
    s32 secs;
    s64 d;

    d = ticks % OSSecondsToTicks(1);
    if (d < 0) {
        d += OSSecondsToTicks(1);
    }
    td->usec = (s32)(OSTicksToMicroseconds(d) % 1000);
    td->msec = (s32)(OSTicksToMilliseconds(d) % 1000);

    ticks -= d;
    days = (s32)(OSTicksToSeconds(ticks) / 86400 + BIAS);
    secs = (s32)(OSTicksToSeconds(ticks) % 86400);
    if (secs < 0) {
        days -= 1;
        secs += 24 * 60 * 60;
    }

    GetDates(days, td);

    td->hour = secs / 60 / 60;
    td->min = (secs / 60) % 60;
    td->sec = secs % 60;
}
