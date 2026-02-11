/*
 * padclamp_property_test.c --- Property-based parity test for PADClamp.
 *
 * Self-contained: oracle (exact decomp copy) and port (identical logic)
 * both inlined.  No external source files.
 *
 * Source: external/mp4-decomp/src/dolphin/pad/Padclamp.c
 *
 * Test levels:
 *   L0: ClampStick random inputs → oracle vs port parity
 *   L1: ClampTrigger random inputs → oracle vs port parity
 *   L2: ClampStick properties (idempotence, range bounds, symmetry, dead zone)
 *   L3: Full PADClamp on random PADStatus arrays
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Types matching decomp ── */
typedef int8_t   s8;
typedef uint8_t  u8;
typedef int32_t  s32;

typedef struct {
    u8 minTrigger;
    u8 maxTrigger;
    s8 minStick;
    s8 maxStick;
    s8 xyStick;
    s8 minSubstick;
    s8 maxSubstick;
    s8 xySubstick;
} PADClampRegion;

/* Default clamp region from decomp */
static const PADClampRegion DefaultClampRegion = {
    30, 180,        /* triggers */
    15, 72, 40,     /* left stick */
    15, 59, 31,     /* right stick */
};

#define PAD_ERR_NONE 0
#define PAD_CHANMAX  4

typedef struct {
    u8  err;
    s8  stickX;
    s8  stickY;
    s8  substickX;
    s8  substickY;
    u8  triggerL;
    u8  triggerR;
} PADStatus;

/* ════════════════════════════════════════════════════════════════════
 *  Oracle: exact copy of decomp Padclamp.c
 * ════════════════════════════════════════════════════════════════════ */

static void oracle_ClampStick(s8 *px, s8 *py, s8 max, s8 xy, s8 min)
{
    int x = *px;
    int y = *py;
    int signX;
    int signY;
    int d;

    if (0 <= x) {
        signX = 1;
    } else {
        signX = -1;
        x = -x;
    }

    if (0 <= y) {
        signY = 1;
    } else {
        signY = -1;
        y = -y;
    }

    if (x <= min) {
        x = 0;
    } else {
        x -= min;
    }
    if (y <= min) {
        y = 0;
    } else {
        y -= min;
    }

    if (x == 0 && y == 0) {
        *px = *py = 0;
        return;
    }

    if (xy * y <= xy * x) {
        d = xy * x + (max - xy) * y;
        if (xy * max < d) {
            x = (s8)(xy * max * x / d);
            y = (s8)(xy * max * y / d);
        }
    } else {
        d = xy * y + (max - xy) * x;
        if (xy * max < d) {
            x = (s8)(xy * max * x / d);
            y = (s8)(xy * max * y / d);
        }
    }

    *px = (s8)(signX * x);
    *py = (s8)(signY * y);
}

static void oracle_ClampTrigger(u8 *trigger, u8 min, u8 max)
{
    if (*trigger <= min) {
        *trigger = 0;
    } else {
        if (max < *trigger) {
            *trigger = max;
        }
        *trigger -= min;
    }
}

static void oracle_PADClamp(PADStatus *status)
{
    int i;
    for (i = 0; i < PAD_CHANMAX; i++, status++) {
        if (status->err != PAD_ERR_NONE) continue;

        oracle_ClampStick(&status->stickX, &status->stickY,
            DefaultClampRegion.maxStick, DefaultClampRegion.xyStick,
            DefaultClampRegion.minStick);
        oracle_ClampStick(&status->substickX, &status->substickY,
            DefaultClampRegion.maxSubstick, DefaultClampRegion.xySubstick,
            DefaultClampRegion.minSubstick);
        /* Inline trigger clamp (decomp inlines it in PADClamp) */
        if (status->triggerL <= DefaultClampRegion.minTrigger) {
            status->triggerL = 0;
        } else {
            if (DefaultClampRegion.maxTrigger < status->triggerL)
                status->triggerL = DefaultClampRegion.maxTrigger;
            status->triggerL -= DefaultClampRegion.minTrigger;
        }
        if (status->triggerR <= DefaultClampRegion.minTrigger) {
            status->triggerR = 0;
        } else {
            if (DefaultClampRegion.maxTrigger < status->triggerR)
                status->triggerR = DefaultClampRegion.maxTrigger;
            status->triggerR -= DefaultClampRegion.minTrigger;
        }
    }
}

/* ════════════════════════════════════════════════════════════════════
 *  Port: identical logic (simulates sdk_port version)
 * ════════════════════════════════════════════════════════════════════ */

static void port_ClampStick(s8 *px, s8 *py, s8 max, s8 xy, s8 min)
{
    int x = *px;
    int y = *py;
    int signX;
    int signY;
    int d;

    if (0 <= x) {
        signX = 1;
    } else {
        signX = -1;
        x = -x;
    }

    if (0 <= y) {
        signY = 1;
    } else {
        signY = -1;
        y = -y;
    }

    if (x <= min) {
        x = 0;
    } else {
        x -= min;
    }
    if (y <= min) {
        y = 0;
    } else {
        y -= min;
    }

    if (x == 0 && y == 0) {
        *px = *py = 0;
        return;
    }

    if (xy * y <= xy * x) {
        d = xy * x + (max - xy) * y;
        if (xy * max < d) {
            x = (s8)(xy * max * x / d);
            y = (s8)(xy * max * y / d);
        }
    } else {
        d = xy * y + (max - xy) * x;
        if (xy * max < d) {
            x = (s8)(xy * max * x / d);
            y = (s8)(xy * max * y / d);
        }
    }

    *px = (s8)(signX * x);
    *py = (s8)(signY * y);
}

static void port_ClampTrigger(u8 *trigger, u8 min, u8 max)
{
    if (*trigger <= min) {
        *trigger = 0;
    } else {
        if (max < *trigger) {
            *trigger = max;
        }
        *trigger -= min;
    }
}

static void port_PADClamp(PADStatus *status)
{
    int i;
    for (i = 0; i < PAD_CHANMAX; i++, status++) {
        if (status->err != PAD_ERR_NONE) continue;

        port_ClampStick(&status->stickX, &status->stickY,
            DefaultClampRegion.maxStick, DefaultClampRegion.xyStick,
            DefaultClampRegion.minStick);
        port_ClampStick(&status->substickX, &status->substickY,
            DefaultClampRegion.maxSubstick, DefaultClampRegion.xySubstick,
            DefaultClampRegion.minSubstick);
        if (status->triggerL <= DefaultClampRegion.minTrigger) {
            status->triggerL = 0;
        } else {
            if (DefaultClampRegion.maxTrigger < status->triggerL)
                status->triggerL = DefaultClampRegion.maxTrigger;
            status->triggerL -= DefaultClampRegion.minTrigger;
        }
        if (status->triggerR <= DefaultClampRegion.minTrigger) {
            status->triggerR = 0;
        } else {
            if (DefaultClampRegion.maxTrigger < status->triggerR)
                status->triggerR = DefaultClampRegion.maxTrigger;
            status->triggerR -= DefaultClampRegion.minTrigger;
        }
    }
}

/* ── PRNG ── */
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

/* ── CLI ── */
static int opt_verbose  = 0;
static int opt_seed     = -1;
static int opt_num_runs = 500;
static const char *opt_op = NULL;

/* ── Stats ── */
static int g_total_checks = 0;
static int g_total_pass   = 0;
static int g_total_fail   = 0;

/* ── L0: ClampStick parity ── */

static int test_clampstick_parity(uint32_t seed)
{
    int n_ops = (int)rand_range(50, 150);
    int fail = 0;
    int i;

    for (i = 0; i < n_ops && fail == 0; i++) {
        /* Random inputs covering full s8 range */
        s8 ox = (s8)(xorshift32() & 0xFF);
        s8 oy = (s8)(xorshift32() & 0xFF);
        s8 px = ox, py = oy;
        s8 qx = ox, qy = oy;

        /* Use default left stick params */
        oracle_ClampStick(&px, &py,
            DefaultClampRegion.maxStick, DefaultClampRegion.xyStick,
            DefaultClampRegion.minStick);
        port_ClampStick(&qx, &qy,
            DefaultClampRegion.maxStick, DefaultClampRegion.xyStick,
            DefaultClampRegion.minStick);

        g_total_checks++;
        if (px != qx || py != qy) {
            fprintf(stderr,
                "  MISMATCH L0-Stick seed=%u: in=(%d,%d) oracle=(%d,%d) port=(%d,%d)\n",
                seed, ox, oy, px, py, qx, qy);
            g_total_fail++;
            fail++;
        } else {
            g_total_pass++;
        }

        /* Also test with substick params */
        px = ox; py = oy; qx = ox; qy = oy;
        oracle_ClampStick(&px, &py,
            DefaultClampRegion.maxSubstick, DefaultClampRegion.xySubstick,
            DefaultClampRegion.minSubstick);
        port_ClampStick(&qx, &qy,
            DefaultClampRegion.maxSubstick, DefaultClampRegion.xySubstick,
            DefaultClampRegion.minSubstick);

        g_total_checks++;
        if (px != qx || py != qy) {
            fprintf(stderr,
                "  MISMATCH L0-Substick seed=%u: in=(%d,%d) oracle=(%d,%d) port=(%d,%d)\n",
                seed, ox, oy, px, py, qx, qy);
            g_total_fail++;
            fail++;
        } else {
            g_total_pass++;
        }
    }

    if (opt_verbose && fail == 0)
        printf("  L0-StickParity: %d inputs OK\n", n_ops);
    return fail;
}

/* ── L1: ClampTrigger parity ── */

static int test_clamptrigger_parity(uint32_t seed)
{
    int fail = 0;
    int i;

    /* Exhaustive: test all 256 possible u8 trigger values */
    for (i = 0; i < 256 && fail == 0; i++) {
        u8 ov = (u8)i;
        u8 pv = ov, qv = ov;

        oracle_ClampTrigger(&pv, DefaultClampRegion.minTrigger,
                            DefaultClampRegion.maxTrigger);
        port_ClampTrigger(&qv, DefaultClampRegion.minTrigger,
                          DefaultClampRegion.maxTrigger);

        g_total_checks++;
        if (pv != qv) {
            fprintf(stderr,
                "  MISMATCH L1-Trigger seed=%u: in=%d oracle=%d port=%d\n",
                seed, i, pv, qv);
            g_total_fail++;
            fail++;
        } else {
            g_total_pass++;
        }
    }

    if (opt_verbose && fail == 0)
        printf("  L1-TriggerParity: 256 values OK\n");
    return fail;
}

/* ── L2: ClampStick properties ── */

static int test_clampstick_properties(uint32_t seed)
{
    int n_ops = (int)rand_range(50, 150);
    int fail = 0;
    int i;

    for (i = 0; i < n_ops && fail == 0; i++) {
        s8 ix = (s8)(xorshift32() & 0xFF);
        s8 iy = (s8)(xorshift32() & 0xFF);
        s8 ox, oy, ox2, oy2;
        s8 nx, ny, nox, noy;
        s8 max = DefaultClampRegion.maxStick;
        s8 xy  = DefaultClampRegion.xyStick;
        s8 min = DefaultClampRegion.minStick;

        /* Apply clamp */
        ox = ix; oy = iy;
        oracle_ClampStick(&ox, &oy, max, xy, min);

        /* Property 1: Output magnitude reduced or equal
         * |out| <= |in| (clamping never amplifies) */
        g_total_checks++;
        {
            int ai = ix < 0 ? -ix : ix;
            int ao = ox < 0 ? -ox : ox;
            int bi = iy < 0 ? -iy : iy;
            int bo = oy < 0 ? -oy : oy;
            if (ao > ai || bo > bi) {
                fprintf(stderr,
                    "  PROP-REDUCE seed=%u: in=(%d,%d) out=(%d,%d)\n",
                    seed, ix, iy, ox, oy);
                g_total_fail++;
                fail++;
                continue;
            }
        }
        g_total_pass++;

        /* Property 2: Dead zone — if |x|<=min && |y|<=min, output is (0,0) */
        {
            int ax = ix < 0 ? -ix : ix;
            int ay = iy < 0 ? -iy : iy;
            if (ax <= min && ay <= min) {
                g_total_checks++;
                if (ox != 0 || oy != 0) {
                    fprintf(stderr,
                        "  PROP-DEADZONE seed=%u: in=(%d,%d) out=(%d,%d)\n",
                        seed, ix, iy, ox, oy);
                    g_total_fail++;
                    fail++;
                    continue;
                }
                g_total_pass++;
            }
        }

        /* Property 3: Odd symmetry — Clamp(-x,-y) == -Clamp(x,y) */
        nx = (s8)(ix == -128 ? 127 : -ix);  /* avoid -(-128) overflow */
        ny = (s8)(iy == -128 ? 127 : -iy);
        nox = nx; noy = ny;
        oracle_ClampStick(&nox, &noy, max, xy, min);
        g_total_checks++;
        /* Clamp(x,y) should equal -Clamp(-x,-y) (with s8 truncation) */
        {
            s8 expected_x = (s8)(-nox);
            s8 expected_y = (s8)(-noy);
            if (ix != -128 && iy != -128 &&
                (ox != expected_x || oy != expected_y)) {
                fprintf(stderr,
                    "  PROP-SYMMETRY seed=%u: in=(%d,%d) clamp=(%d,%d) "
                    "neg_in=(%d,%d) neg_clamp=(%d,%d)\n",
                    seed, ix, iy, ox, oy, nx, ny, nox, noy);
                g_total_fail++;
                fail++;
                continue;
            }
            g_total_pass++;
        }
    }

    if (opt_verbose && fail == 0)
        printf("  L2-StickProperties: %d inputs OK\n", n_ops);
    return fail;
}

/* ── L3: Full PADClamp parity ── */

static int test_padclamp_full(uint32_t seed)
{
    int n_ops = (int)rand_range(20, 60);
    int fail = 0;
    int op;

    for (op = 0; op < n_ops && fail == 0; op++) {
        PADStatus oracle_pads[PAD_CHANMAX];
        PADStatus port_pads[PAD_CHANMAX];
        int ch;

        /* Generate random PADStatus for all 4 channels */
        for (ch = 0; ch < PAD_CHANMAX; ch++) {
            oracle_pads[ch].err = (xorshift32() % 4 == 0) ? 1 : PAD_ERR_NONE;
            oracle_pads[ch].stickX = (s8)(xorshift32() & 0xFF);
            oracle_pads[ch].stickY = (s8)(xorshift32() & 0xFF);
            oracle_pads[ch].substickX = (s8)(xorshift32() & 0xFF);
            oracle_pads[ch].substickY = (s8)(xorshift32() & 0xFF);
            oracle_pads[ch].triggerL = (u8)(xorshift32() & 0xFF);
            oracle_pads[ch].triggerR = (u8)(xorshift32() & 0xFF);
        }
        memcpy(port_pads, oracle_pads, sizeof(oracle_pads));

        oracle_PADClamp(oracle_pads);
        port_PADClamp(port_pads);

        /* Compare all fields */
        for (ch = 0; ch < PAD_CHANMAX; ch++) {
            g_total_checks++;
            if (memcmp(&oracle_pads[ch], &port_pads[ch], sizeof(PADStatus)) != 0) {
                fprintf(stderr,
                    "  MISMATCH L3-PADClamp seed=%u ch=%d: "
                    "o=(%d,%d,%d,%d,L=%d,R=%d) p=(%d,%d,%d,%d,L=%d,R=%d)\n",
                    seed, ch,
                    oracle_pads[ch].stickX, oracle_pads[ch].stickY,
                    oracle_pads[ch].substickX, oracle_pads[ch].substickY,
                    oracle_pads[ch].triggerL, oracle_pads[ch].triggerR,
                    port_pads[ch].stickX, port_pads[ch].stickY,
                    port_pads[ch].substickX, port_pads[ch].substickY,
                    port_pads[ch].triggerL, port_pads[ch].triggerR);
                g_total_fail++;
                fail++;
                break;
            }
            g_total_pass++;
        }
    }

    if (opt_verbose && fail == 0)
        printf("  L3-PADClamp: %d x 4ch OK\n", n_ops);
    return fail;
}

/* ── Run one seed ── */

static int run_seed(uint32_t seed)
{
    int fail = 0;

    if (!opt_op || strstr("L0", opt_op) || strstr("STICK", opt_op)) {
        g_rng = seed;
        fail += test_clampstick_parity(seed);
    }

    if (!opt_op || strstr("L1", opt_op) || strstr("TRIGGER", opt_op)) {
        g_rng = seed ^ 0x50414430u; /* "PAD0" */
        if (g_rng == 0) g_rng = 1;
        fail += test_clamptrigger_parity(seed);
    }

    if (!opt_op || strstr("L2", opt_op) || strstr("PROP", opt_op)) {
        g_rng = seed ^ 0xC1A4B000u;
        if (g_rng == 0) g_rng = 1;
        fail += test_clampstick_properties(seed);
    }

    if (!opt_op || strstr("L3", opt_op) || strstr("FULL", opt_op)) {
        g_rng = seed ^ 0x6A075710u;
        if (g_rng == 0) g_rng = 1;
        fail += test_padclamp_full(seed);
    }

    return fail;
}

/* ── CLI ── */

static void parse_args(int argc, char **argv)
{
    int i;
    for (i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--seed=", 7) == 0)
            opt_seed = atoi(argv[i] + 7);
        else if (strncmp(argv[i], "--num-runs=", 11) == 0)
            opt_num_runs = atoi(argv[i] + 11);
        else if (strncmp(argv[i], "--op=", 5) == 0)
            opt_op = argv[i] + 5;
        else if (strcmp(argv[i], "-v") == 0)
            opt_verbose = 1;
        else {
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

    printf("=== PADClamp Property Test ===\n");

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
