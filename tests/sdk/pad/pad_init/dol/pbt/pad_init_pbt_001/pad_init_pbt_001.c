#include <stdint.h>

#include "src/sdk_port/gc_mem.c"
#include "src/sdk_port/sdk_state.h"

typedef uint16_t u16;
typedef uint32_t u32;
typedef int BOOL;

BOOL PADInit(void);

extern u32 gc_pad_initialized;
extern u32 gc_pad_si_refresh_calls;
extern u32 gc_pad_register_reset_calls;
extern u32 gc_pad_reset_mask;
extern u32 gc_pad_cmd_probe_device[4];
extern u32 gc_pad_spec;
extern u32 gc_pad_fix_bits;
extern u16 gc_os_wireless_pad_fix_mode;

static inline void store_u32be_ptr(volatile uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v >> 0);
}

static inline uint32_t rotl1(uint32_t v) {
    return (v << 1) | (v >> 31);
}

static inline void reset_env(uint32_t init, uint32_t spec, uint32_t fix, uint16_t wmode,
                             uint32_t si_calls, uint32_t rr_calls, uint32_t reset_mask) {
    gc_pad_initialized = init;
    gc_pad_spec = spec;
    gc_pad_fix_bits = fix;
    gc_os_wireless_pad_fix_mode = wmode;
    gc_pad_si_refresh_calls = si_calls;
    gc_pad_register_reset_calls = rr_calls;
    gc_pad_reset_mask = reset_mask;
    for (int i = 0; i < 4; i++) gc_pad_cmd_probe_device[i] = 0;
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_INITIALIZED, init);
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RECALIBRATE_BITS, 0u);
}

static inline uint32_t hash_state(uint32_t ret) {
    uint32_t h = ret;
    h = rotl1(h) ^ gc_pad_initialized;
    h = rotl1(h) ^ gc_pad_si_refresh_calls;
    h = rotl1(h) ^ gc_pad_register_reset_calls;
    h = rotl1(h) ^ gc_pad_reset_mask;
    h = rotl1(h) ^ gc_pad_cmd_probe_device[0];
    h = rotl1(h) ^ gc_pad_cmd_probe_device[1];
    h = rotl1(h) ^ gc_pad_cmd_probe_device[2];
    h = rotl1(h) ^ gc_pad_cmd_probe_device[3];
    h = rotl1(h) ^ gc_pad_spec;
    h = rotl1(h) ^ (u32)gc_os_wireless_pad_fix_mode;
    h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_SPEC);
    h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MAKE_STATUS_KIND);
    h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RECALIBRATE_BITS);
    return h;
}

static inline void dump_state(volatile uint8_t *out, uint32_t *off, uint32_t ret) {
    store_u32be_ptr(out + (*off) * 4u, ret); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_pad_initialized); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_pad_si_refresh_calls); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_pad_register_reset_calls); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_pad_reset_mask); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_pad_cmd_probe_device[0]); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_pad_cmd_probe_device[1]); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_pad_cmd_probe_device[2]); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_pad_cmd_probe_device[3]); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_pad_spec); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, (u32)gc_os_wireless_pad_fix_mode); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_SPEC)); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MAKE_STATUS_KIND)); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RECALIBRATE_BITS)); (*off)++;
}

static uint32_t rng_state;
static uint32_t rng_next(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static const uint32_t k_specs[] = {0u, 1u, 5u, 0xffffffffu};

enum {
    L0_CASES = 2 * 2 * 4,
    L1_CASES = 4 * 4,
    L2_CASES = 4,
    L3_CASES = 512,
    L4_CASES = 4,
    L5_CASES = 4,
};

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);
    gc_sdk_state_reset();

    volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
    uint32_t word_off = 0u;
    uint32_t total_cases = 0u;
    uint32_t master_hash = 0u;
    uint32_t l0_hash = 0u, l1_hash = 0u, l2_hash = 0u, l3_hash = 0u, l4_hash = 0u, l5_hash = 0u;

    // L0 isolated matrix.
    for (uint32_t init = 0; init < 2u; init++) {
        for (uint32_t fix = 0; fix < 2u; fix++) {
            for (uint32_t si = 0; si < 4u; si++) {
                reset_env(init, k_specs[si], fix ? 0x10u : 0u, 0u, 0u, 0u, 0u);
                uint32_t ret = (uint32_t)PADInit();
                l0_hash = rotl1(l0_hash) ^ hash_state(ret);
            }
        }
    }
    total_cases += L0_CASES;
    master_hash = rotl1(master_hash) ^ l0_hash;

    // L1 accumulation from clean state.
    for (uint32_t s0 = 0; s0 < 4u; s0++) {
        for (uint32_t s1 = 0; s1 < 4u; s1++) {
            reset_env(0u, k_specs[s0], 0u, 0u, 0u, 0u, 0u);
            uint32_t r0 = (uint32_t)PADInit();
            gc_pad_initialized = 0u;
            gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_INITIALIZED, 0u);
            gc_pad_spec = k_specs[s1];
            uint32_t r1 = (uint32_t)PADInit();
            l1_hash = rotl1(l1_hash) ^ hash_state(r0 ^ (r1 << 1));
        }
    }
    total_cases += L1_CASES;
    master_hash = rotl1(master_hash) ^ l1_hash;

    // L2 idempotency (second call with initialized already set).
    for (uint32_t s = 0; s < 4u; s++) {
        reset_env(0u, k_specs[s], 0u, 0u, 0u, 0u, 0u);
        (void)PADInit();
        uint32_t before = hash_state(0u);
        uint32_t ret2 = (uint32_t)PADInit();
        l2_hash = rotl1(l2_hash) ^ before ^ hash_state(ret2);
    }
    total_cases += L2_CASES;
    master_hash = rotl1(master_hash) ^ l2_hash;

    // L3 random-start.
    rng_state = 0x77AACC11u;
    for (uint32_t i = 0; i < L3_CASES; i++) {
        reset_env(rng_next() & 1u, k_specs[rng_next() % 4u], rng_next(), (u16)(rng_next() & 0x3fffu),
                  rng_next(), rng_next(), rng_next());
        uint32_t ret = (uint32_t)PADInit();
        l3_hash = rotl1(l3_hash) ^ hash_state(ret);
    }
    total_cases += L3_CASES;
    master_hash = rotl1(master_hash) ^ l3_hash;

    // L4 harvest replay style (spec=5, clean init path).
    reset_env(0u, 5u, 0u, 0u, 0u, 0u, 0u);
    for (uint32_t i = 0; i < L4_CASES; i++) {
        gc_pad_initialized = 0u;
        gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_INITIALIZED, 0u);
        uint32_t ret = (uint32_t)PADInit();
        l4_hash = rotl1(l4_hash) ^ hash_state(ret);
    }
    total_cases += L4_CASES;
    master_hash = rotl1(master_hash) ^ l4_hash;

    // L5 boundary.
    reset_env(1u, 5u, 0xffffffffu, 0x3fffu, 0x12345678u, 0x9abcdef0u, 0x11111111u);
    uint32_t r0 = (uint32_t)PADInit();
    l5_hash = rotl1(l5_hash) ^ hash_state(r0);
    reset_env(0u, 0xffffffffu, 1u, 0u, 0u, 0u, 0u);
    uint32_t r1 = (uint32_t)PADInit();
    l5_hash = rotl1(l5_hash) ^ hash_state(r1);
    reset_env(0u, 0u, 1u, 0x3fffu, 0u, 0u, 0u);
    uint32_t r2 = (uint32_t)PADInit();
    l5_hash = rotl1(l5_hash) ^ hash_state(r2);
    reset_env(0u, 1u, 0u, 0u, 0u, 0u, 0u);
    uint32_t r3 = (uint32_t)PADInit();
    l5_hash = rotl1(l5_hash) ^ hash_state(r3);
    total_cases += L5_CASES;
    master_hash = rotl1(master_hash) ^ l5_hash;

    store_u32be_ptr(out + word_off * 4u, 0x50424954u); word_off++; // PBIT
    store_u32be_ptr(out + word_off * 4u, total_cases); word_off++;
    store_u32be_ptr(out + word_off * 4u, master_hash); word_off++;
    store_u32be_ptr(out + word_off * 4u, l0_hash); word_off++;
    store_u32be_ptr(out + word_off * 4u, L0_CASES); word_off++;
    store_u32be_ptr(out + word_off * 4u, l1_hash); word_off++;
    store_u32be_ptr(out + word_off * 4u, L1_CASES); word_off++;
    store_u32be_ptr(out + word_off * 4u, l2_hash); word_off++;
    store_u32be_ptr(out + word_off * 4u, L2_CASES); word_off++;
    store_u32be_ptr(out + word_off * 4u, l3_hash); word_off++;
    store_u32be_ptr(out + word_off * 4u, L3_CASES); word_off++;
    store_u32be_ptr(out + word_off * 4u, l4_hash); word_off++;
    store_u32be_ptr(out + word_off * 4u, L4_CASES); word_off++;
    store_u32be_ptr(out + word_off * 4u, l5_hash); word_off++;
    store_u32be_ptr(out + word_off * 4u, L5_CASES); word_off++;

    reset_env(0u, 5u, 0u, 0u, 0u, 0u, 0u);
    dump_state(out, &word_off, (uint32_t)PADInit());
    reset_env(0u, 0u, 1u, 0u, 0u, 0u, 0u);
    dump_state(out, &word_off, (uint32_t)PADInit());
    reset_env(1u, 5u, 0u, 0u, 0u, 0u, 0u);
    dump_state(out, &word_off, (uint32_t)PADInit());
    reset_env(0u, 1u, 0u, 0u, 0u, 0u, 0u);
    dump_state(out, &word_off, (uint32_t)PADInit());

    while (1) {
        __asm__ volatile("nop");
    }
    return 0;
}
