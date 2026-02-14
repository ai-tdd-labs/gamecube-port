#include <stdint.h>
#include <string.h>

// Minimal SRAM/RTC port for deterministic host scenarios.
//
// Evidence (layout + lock/unlock semantics): decomp_mario_party_4/include/dolphin/OSRtcPriv.h
// and decomp_mario_party_4/src/dolphin/os/OSRtc.c.

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int64_t s64;
typedef int BOOL;
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

int OSDisableInterrupts(void);
int OSRestoreInterrupts(int level);

u8 gc_sram_flags;
u32 gc_sram_unlock_calls;
u64 gc_os_tick_counter;

// Deterministic knobs (test-only): allow modeling ReadSram/WriteSram failure.
u32 gc_os_sram_read_ok = 1;
u32 gc_os_sram_write_ok = 1;

// Test-only instrumentation: export modeled internal state.
u32 gc_os_scb_locked;
u32 gc_os_scb_enabled;
u32 gc_os_scb_sync;
u32 gc_os_scb_offset;
u32 gc_os_sram_read_calls;
u32 gc_os_sram_write_calls;

#define RTC_SRAM_SIZE 64u

typedef struct OSSram {
    uint16_t checkSum;
    uint16_t checkSumInv;
    uint32_t ead0;
    uint32_t ead1;
    uint32_t counterBias;
    int8_t displayOffsetH;
    u8 ntd;
    u8 language;
    u8 flags;
} OSSram;

typedef struct OSSramEx {
    u8 flashID[2][12];
    u32 wirelessKeyboardID;
    uint16_t wirelessPadID[4];
    u8 dvdErrorCode;
    u8 _padding0;
    u8 flashIDCheckSum[2];
    uint16_t gbs;
    u8 _padding1[2];
} OSSramEx;

typedef struct {
    u8 sram[RTC_SRAM_SIZE];
    u32 offset;
    BOOL enabled;
    BOOL locked;
    BOOL sync;
} ScbT;

static ScbT Scb;

static inline void update_mirrors(void) {
    gc_os_scb_locked = (u32)(Scb.locked != FALSE);
    gc_os_scb_enabled = (u32)(Scb.enabled != FALSE);
    gc_os_scb_sync = (u32)(Scb.sync != FALSE);
    gc_os_scb_offset = Scb.offset;
}

static BOOL ReadSram(u8* buffer) {
    gc_os_sram_read_calls++;
    if (!gc_os_sram_read_ok) return FALSE;
    memset(buffer, 0, RTC_SRAM_SIZE);
    return TRUE;
}

static BOOL WriteSram(const u8* buffer, u32 offset, u32 size) {
    (void)buffer;
    (void)offset;
    (void)size;
    gc_os_sram_write_calls++;
    return gc_os_sram_write_ok ? TRUE : FALSE;
}

void __OSInitSram(void) {
    Scb.locked = Scb.enabled = FALSE;
    Scb.sync = ReadSram(Scb.sram);
    Scb.offset = RTC_SRAM_SIZE;
    update_mirrors();
}

static void* LockSram(u32 offset) {
    BOOL enabled = OSDisableInterrupts();

    if (Scb.locked != FALSE) {
        OSRestoreInterrupts(enabled);
        update_mirrors();
        return NULL;
    }

    Scb.enabled = enabled;
    Scb.locked = TRUE;
    update_mirrors();
    return Scb.sram + offset;
}

OSSram* __OSLockSram(void) {
    OSSram* s = (OSSram*)LockSram(0);
    if (s) {
        // Maintain legacy test knob used by OSGetProgressiveMode suites.
        s->flags = gc_sram_flags;
    }
    return s;
}

OSSram* __OSLockSramHACK(void) { return __OSLockSram(); }

OSSramEx* __OSLockSramEx(void) { return (OSSramEx*)LockSram((u32)sizeof(OSSram)); }

static BOOL UnlockSram(BOOL commit, u32 offset) {
    uint16_t* p;

    if (commit) {
        if (offset == 0) {
            OSSram* sram = (OSSram*)Scb.sram;
            sram->checkSum = 0;
            sram->checkSumInv = 0;
            for (p = (uint16_t*)&sram->counterBias; p < (uint16_t*)(Scb.sram + sizeof(OSSram)); p++) {
                sram->checkSum += *p;
                sram->checkSumInv += (uint16_t)~*p;
            }
        }

        if (offset < Scb.offset) {
            Scb.offset = offset;
        }

        Scb.sync = WriteSram(Scb.sram + Scb.offset, Scb.offset, RTC_SRAM_SIZE - Scb.offset);
        if (Scb.sync) {
            Scb.offset = RTC_SRAM_SIZE;
        }
    }

    Scb.locked = FALSE;
    // Keep gc_sram_flags in sync with SRAM flags field regardless of commit.
    gc_sram_flags = ((OSSram*)Scb.sram)->flags;
    gc_sram_unlock_calls++;

    OSRestoreInterrupts(Scb.enabled);
    update_mirrors();
    return Scb.sync;
}

BOOL __OSUnlockSram(BOOL commit) { return UnlockSram(commit, 0); }
BOOL __OSUnlockSramEx(BOOL commit) { return UnlockSram(commit, (u32)sizeof(OSSram)); }

BOOL __OSSyncSram(void) {
    update_mirrors();
    return Scb.sync;
}

u32 OSGetProgressiveMode(void) {
    OSSram *sram;
    u32 mode;

    sram = __OSLockSramHACK();
    mode = (sram->flags & 0x80u) >> 7;
    (void)__OSUnlockSram(FALSE);
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
