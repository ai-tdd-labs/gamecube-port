#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef int64_t s64;
typedef uint32_t u32;

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

u32 OSGetTick(void);
void OSTicksToCalendarTime(s64 ticks, OSCalendarTime *td);

static int g_failures = 0;

static void expect_u32(const char *label, u32 got, u32 want) {
    if (got != want) {
        fprintf(stderr, "FAIL %s: got=%u want=%u\n", label, got, want);
        g_failures++;
    }
}

static void expect_i32(const char *label, int32_t got, int32_t want) {
    if (got != want) {
        fprintf(stderr, "FAIL %s: got=%d want=%d\n", label, got, want);
        g_failures++;
    }
}

int main(void) {
    OSCalendarTime t;
    u32 a, b, c;

    a = OSGetTick();
    b = OSGetTick();
    c = OSGetTick();
    expect_u32("OSGetTick step a->b", b - a, 40500u);
    expect_u32("OSGetTick step b->c", c - b, 40500u);

    OSTicksToCalendarTime(0, &t);
    expect_i32("epoch.year", t.year, 2000);
    expect_i32("epoch.mon", t.mon, 0);
    expect_i32("epoch.mday", t.mday, 1);
    expect_i32("epoch.hour", t.hour, 0);
    expect_i32("epoch.min", t.min, 0);
    expect_i32("epoch.sec", t.sec, 0);
    expect_i32("epoch.msec", t.msec, 0);
    expect_i32("epoch.usec", t.usec, 0);

    OSTicksToCalendarTime(-1, &t);
    expect_i32("neg1.year", t.year, 1999);
    expect_i32("neg1.mon", t.mon, 11);
    expect_i32("neg1.mday", t.mday, 31);
    expect_i32("neg1.hour", t.hour, 23);
    expect_i32("neg1.min", t.min, 59);
    expect_i32("neg1.sec", t.sec, 59);
    expect_i32("neg1.msec", t.msec, 999);
    expect_i32("neg1.usec", t.usec, 999);

    if (g_failures != 0) {
        fprintf(stderr, "ostime_sdk_port_unit_test: %d failure(s)\n", g_failures);
        return 1;
    }

    printf("ostime_sdk_port_unit_test: PASS\n");
    return 0;
}
