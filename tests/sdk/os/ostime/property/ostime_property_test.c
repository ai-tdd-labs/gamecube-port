/*
 * ostime_property_test.c --- Property-based parity test for OSTicksToCalendarTime.
 *
 * Self-contained: oracle (exact decomp copy) and port (identical logic,
 * different function name) both inlined here.  No external source files.
 *
 * GC timer clock = bus_clock / 4 = 162,000,000 / 4 = 40,500,000 Hz.
 *
 * Test levels:
 *   L0: Random ticks → compare oracle vs port calendar fields
 *   L1: Edge cases (epoch, negative, leap years, year boundaries)
 *   L2: Monotonicity (t1 < t2 → calendar(t1) <= calendar(t2))
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── GC tick rate constants ── */
#define GC_BUS_CLOCK      162000000u
#define GC_TIMER_CLOCK    (GC_BUS_CLOCK / 4)  /* 40,500,000 */

#define TICKS_PER_SEC     ((int64_t)GC_TIMER_CLOCK)
#define TICKS_PER_MSEC    ((int64_t)(GC_TIMER_CLOCK / 1000))
#define TICKS_PER_USEC_NUM 8
#define TICKS_PER_USEC_DEN ((int64_t)(GC_TIMER_CLOCK / 125000))

/* Decomp macros */
#define OSSecondsToTicks(sec)          ((int64_t)(sec) * TICKS_PER_SEC)
#define OSTicksToSeconds(ticks)        ((ticks) / TICKS_PER_SEC)
#define OSTicksToMilliseconds(ticks)   ((ticks) / TICKS_PER_MSEC)
#define OSTicksToMicroseconds(ticks)   (((ticks) * TICKS_PER_USEC_NUM) / TICKS_PER_USEC_DEN)

/* ── Calendar time struct ── */
typedef struct {
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
} CalendarTime;

/* ── Shared helpers (exact decomp logic) ── */

#define OS_TIME_MONTH_MAX    12
#define OS_TIME_WEEK_DAY_MAX  7
#define OS_TIME_YEAR_DAY_MAX 365

static int32_t YearDays[OS_TIME_MONTH_MAX] =
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
static int32_t LeapYearDays[OS_TIME_MONTH_MAX] =
    { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 };

static int IsLeapYear(int32_t year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static int32_t GetLeapDays(int32_t year)
{
    if (year < 1) return 0;
    return (year + 3) / 4 - (year - 1) / 100 + (year - 1) / 400;
}

static void GetDates(int32_t days, CalendarTime *cal)
{
    int32_t year;
    int32_t totalDays;
    int32_t *p_days;
    int32_t month;

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

/* ════════════════════════════════════════════════════════════════════
 *  Oracle: exact copy of decomp OSTicksToCalendarTime
 * ════════════════════════════════════════════════════════════════════ */

static void oracle_OSTicksToCalendarTime(int64_t ticks, CalendarTime *td)
{
    int32_t days;
    int32_t secs;
    int64_t d;

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

/* ════════════════════════════════════════════════════════════════════
 *  Port: linked from src/sdk_port/os/OSTime.c
 *  CalendarTime layout matches OSCalendarTime in OSTime.c (10 x int32_t).
 * ════════════════════════════════════════════════════════════════════ */

void OSTicksToCalendarTime(int64_t ticks, CalendarTime *td);
#define port_OSTicksToCalendarTime(ticks, td) OSTicksToCalendarTime((ticks), (td))

/* ── PRNG (xorshift32) ── */
static uint32_t g_rng;

static uint32_t xorshift32(void)
{
    uint32_t x = g_rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_rng = x;
    return x;
}

static uint32_t rand_range(uint32_t lo, uint32_t hi)
{
    return lo + xorshift32() % (hi - lo + 1);
}

/* Generate a random 64-bit tick value across interesting ranges */
static int64_t rand_ticks(void)
{
    uint32_t range = xorshift32() % 6;
    int64_t t;

    switch (range) {
    case 0: /* small positive (< 1 day) */
        t = (int64_t)xorshift32() * (int64_t)(xorshift32() % 86400);
        break;
    case 1: /* medium (0-10 years from epoch in ticks) */
        t = (int64_t)xorshift32() * (int64_t)xorshift32();
        if (t < 0) t = -t;
        t %= (int64_t)10 * 365 * 86400 * TICKS_PER_SEC;
        break;
    case 2: /* large positive (0-200 years) */
        t = (int64_t)xorshift32() * (int64_t)xorshift32();
        if (t < 0) t = -t;
        t %= (int64_t)200 * 365 * 86400 * TICKS_PER_SEC;
        break;
    case 3: /* negative (before epoch) */
        t = -(int64_t)(xorshift32()) * (int64_t)(xorshift32() % 86400);
        break;
    case 4: /* near zero */
        t = (int64_t)(int32_t)xorshift32();
        break;
    default: /* exact seconds boundary + small offset */
        t = OSSecondsToTicks((int64_t)(int32_t)xorshift32()) +
            (int64_t)(xorshift32() % GC_TIMER_CLOCK);
        break;
    }
    return t;
}

/* ── CLI options ── */
static int opt_verbose  = 0;
static int opt_seed     = -1;
static int opt_num_runs = 500;
static const char *opt_op = NULL;

/* ── Statistics ── */
static int g_total_checks = 0;
static int g_total_pass   = 0;
static int g_total_fail   = 0;

/* ── Comparison helper ── */

static int compare_cal(const char *label, int64_t ticks,
                       const CalendarTime *o, const CalendarTime *p,
                       uint32_t seed)
{
    if (o->sec  != p->sec  || o->min  != p->min  ||
        o->hour != p->hour || o->mday != p->mday ||
        o->mon  != p->mon  || o->year != p->year ||
        o->wday != p->wday || o->yday != p->yday ||
        o->msec != p->msec || o->usec != p->usec)
    {
        fprintf(stderr,
            "  MISMATCH %s seed=%u ticks=%lld:\n"
            "    oracle: %d-%02d-%02d %02d:%02d:%02d.%03d.%03d wday=%d yday=%d\n"
            "    port:   %d-%02d-%02d %02d:%02d:%02d.%03d.%03d wday=%d yday=%d\n",
            label, seed, (long long)ticks,
            o->year, o->mon+1, o->mday, o->hour, o->min, o->sec,
            o->msec, o->usec, o->wday, o->yday,
            p->year, p->mon+1, p->mday, p->hour, p->min, p->sec,
            p->msec, p->usec, p->wday, p->yday);
        return 1;
    }
    return 0;
}

static int check_ticks(const char *label, int64_t ticks, uint32_t seed)
{
    CalendarTime o, p;
    memset(&o, 0, sizeof(o));
    memset(&p, 0, sizeof(p));

    oracle_OSTicksToCalendarTime(ticks, &o);
    port_OSTicksToCalendarTime(ticks, &p);

    g_total_checks++;
    if (compare_cal(label, ticks, &o, &p, seed)) {
        g_total_fail++;
        return 1;
    }
    g_total_pass++;
    return 0;
}

/* ── L0: Random ticks ── */

static int test_random_ticks(uint32_t seed)
{
    int n_ops = (int)rand_range(40, 100);
    int fail = 0;
    int i;

    for (i = 0; i < n_ops && fail == 0; i++) {
        int64_t ticks = rand_ticks();
        fail += check_ticks("L0-Random", ticks, seed);
    }

    if (opt_verbose && fail == 0)
        printf("  L0-Random: %d ticks OK\n", n_ops);

    return fail;
}

/* ── L1: Edge cases ── */

static int test_edge_cases(uint32_t seed)
{
    int fail = 0;
    int year;

    /* Epoch (ticks=0 = 2000-01-01 00:00:00.000.000) */
    fail += check_ticks("L1-Epoch", 0, seed);
    if (fail) return fail;

    /* 1 tick before epoch */
    fail += check_ticks("L1-PreEpoch", -1, seed);
    if (fail) return fail;

    /* 1 second exactly */
    fail += check_ticks("L1-1sec", OSSecondsToTicks(1), seed);
    if (fail) return fail;

    /* 1 day exactly */
    fail += check_ticks("L1-1day", OSSecondsToTicks(86400), seed);
    if (fail) return fail;

    /* End of 2000-02-28 (leap year) */
    fail += check_ticks("L1-Feb28",
        OSSecondsToTicks((int64_t)(31 + 28) * 86400 - 1), seed);
    if (fail) return fail;

    /* 2000-02-29 (leap day) */
    fail += check_ticks("L1-Feb29",
        OSSecondsToTicks((int64_t)(31 + 29) * 86400), seed);
    if (fail) return fail;

    /* 2000-03-01 */
    fail += check_ticks("L1-Mar1",
        OSSecondsToTicks((int64_t)(31 + 29 + 1) * 86400), seed);
    if (fail) return fail;

    /* Year boundaries for several years */
    for (year = 0; year <= 30; year++) {
        int32_t days_in_year = 365 + (IsLeapYear(2000 + year) ? 1 : 0);
        /* We approximate the tick offset; the exact calculation uses BIAS */
        int64_t t = OSSecondsToTicks((int64_t)year * 365 * 86400);
        fail += check_ticks("L1-YearBoundary", t, seed);
        if (fail) return fail;
        /* Also test mid-year */
        t = OSSecondsToTicks(((int64_t)year * 365 + days_in_year / 2) * 86400);
        fail += check_ticks("L1-MidYear", t, seed);
        if (fail) return fail;
    }

    /* Negative (before 2000) */
    fail += check_ticks("L1-Neg1day", OSSecondsToTicks(-86400), seed);
    if (fail) return fail;
    fail += check_ticks("L1-Neg1year", OSSecondsToTicks(-(int64_t)365 * 86400), seed);
    if (fail) return fail;

    /* Large negative (1900) */
    fail += check_ticks("L1-1900",
        OSSecondsToTicks(-(int64_t)100 * 365 * 86400), seed);
    if (fail) return fail;

    /* Subsecond precision */
    {
        int64_t t = OSSecondsToTicks(3600) + TICKS_PER_MSEC * 500 + 100;
        fail += check_ticks("L1-Subsec", t, seed);
    }

    if (opt_verbose && fail == 0)
        printf("  L1-EdgeCases: OK\n");

    return fail;
}

/* ── L2: Monotonicity ── */

static int test_monotonicity(uint32_t seed)
{
    int n_ops = (int)rand_range(30, 80);
    int fail = 0;
    int i;
    int64_t prev_ticks;
    CalendarTime prev_cal, cur_cal;

    prev_ticks = rand_ticks();
    memset(&prev_cal, 0, sizeof(prev_cal));
    oracle_OSTicksToCalendarTime(prev_ticks, &prev_cal);

    for (i = 0; i < n_ops && fail == 0; i++) {
        /* Generate a tick value >= prev */
        int64_t delta = (int64_t)(xorshift32() % 1000000) *
                        (int64_t)(xorshift32() % GC_TIMER_CLOCK);
        int64_t cur_ticks = prev_ticks + (delta < 0 ? -delta : delta);

        memset(&cur_cal, 0, sizeof(cur_cal));
        oracle_OSTicksToCalendarTime(cur_ticks, &cur_cal);

        /* Oracle vs port parity */
        fail += check_ticks("L2-Mono", cur_ticks, seed);
        if (fail) break;

        /* Monotonicity: year >= prev year, or if same year, yday >= prev yday */
        if (cur_cal.year < prev_cal.year ||
            (cur_cal.year == prev_cal.year && cur_cal.yday < prev_cal.yday) ||
            (cur_cal.year == prev_cal.year && cur_cal.yday == prev_cal.yday &&
             cur_cal.hour < prev_cal.hour))
        {
            fprintf(stderr,
                "  MONO FAIL seed=%u: "
                "prev=%d-%03d %02d:%02d cur=%d-%03d %02d:%02d\n",
                seed,
                prev_cal.year, prev_cal.yday, prev_cal.hour, prev_cal.min,
                cur_cal.year, cur_cal.yday, cur_cal.hour, cur_cal.min);
            g_total_fail++;
            g_total_checks++;
            fail++;
            break;
        }
        g_total_pass++;
        g_total_checks++;

        prev_ticks = cur_ticks;
        prev_cal = cur_cal;
    }

    if (opt_verbose && fail == 0)
        printf("  L2-Monotonicity: %d steps OK\n", n_ops);

    return fail;
}

/* ── Run one seed ── */

static int run_seed(uint32_t seed)
{
    int fail = 0;

    if (!opt_op || strstr("L0", opt_op) || strstr("RANDOM", opt_op)) {
        g_rng = seed;
        fail += test_random_ticks(seed);
    }

    if (!opt_op || strstr("L1", opt_op) || strstr("EDGE", opt_op)) {
        g_rng = seed ^ 0x54494D45u; /* "TIME" */
        if (g_rng == 0) g_rng = 1;
        fail += test_edge_cases(seed);
    }

    if (!opt_op || strstr("L2", opt_op) || strstr("MONO", opt_op)) {
        g_rng = seed ^ 0xCA1E4DA7u;
        if (g_rng == 0) g_rng = 1;
        fail += test_monotonicity(seed);
    }

    return fail;
}

/* ── CLI ── */

static void parse_args(int argc, char **argv)
{
    int i;
    for (i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--seed=", 7) == 0) {
            opt_seed = atoi(argv[i] + 7);
        } else if (strncmp(argv[i], "--num-runs=", 11) == 0) {
            opt_num_runs = atoi(argv[i] + 11);
        } else if (strncmp(argv[i], "--op=", 5) == 0) {
            opt_op = argv[i] + 5;
        } else if (strcmp(argv[i], "-v") == 0) {
            opt_verbose = 1;
        } else {
            fprintf(stderr, "Unknown arg: %s\n", argv[i]);
            exit(1);
        }
    }
}

int main(int argc, char **argv)
{
    int total_seeds, fails;
    uint32_t s;

    parse_args(argc, argv);

    printf("=== OSTime Property Test ===\n");

    if (opt_seed >= 0) {
        total_seeds = 1;
        fails = run_seed((uint32_t)opt_seed);
    } else {
        total_seeds = opt_num_runs;
        fails = 0;
        for (s = 1; s <= (uint32_t)opt_num_runs; s++) {
            if (s % 100 == 0 || opt_verbose)
                printf("  progress: seed %u/%d\n", s, opt_num_runs);
            fails += run_seed(s);
        }
    }

    printf("\n--- Summary ---\n");
    printf("Seeds:  %d\n", total_seeds);
    printf("Checks: %d  (pass=%d  fail=%d)\n",
           g_total_checks, g_total_pass, g_total_fail);

    if (g_total_fail == 0) {
        printf("\nRESULT: %d/%d PASS\n", g_total_pass, g_total_checks);
        return 0;
    } else {
        printf("\nRESULT: FAIL (%d failures)\n", g_total_fail);
        return 1;
    }
}
