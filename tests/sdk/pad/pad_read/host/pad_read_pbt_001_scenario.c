#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "sdk_state.h"

typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef uint8_t u8;

typedef struct PADStatus {
    u16 button;
    s8 stickX;
    s8 stickY;
    s8 substickX;
    s8 substickY;
    u8 triggerL;
    u8 triggerR;
    u8 analogA;
    u8 analogB;
    s8 err;
    u8 _pad;
} PADStatus;

u32 PADRead(PADStatus *status);

static inline uint32_t rotl1(uint32_t v) {
    return (v << 1) | (v >> 31);
}

static inline uint32_t hash_status_fields(const PADStatus *st) {
    uint32_t h = 0u;
    for (int i = 0; i < 4; i++) {
        h = rotl1(h) ^ st[i].button;
        h = rotl1(h) ^ (uint32_t)(uint8_t)st[i].stickX;
        h = rotl1(h) ^ (uint32_t)(uint8_t)st[i].stickY;
        h = rotl1(h) ^ (uint32_t)(uint8_t)st[i].substickX;
        h = rotl1(h) ^ (uint32_t)(uint8_t)st[i].substickY;
        h = rotl1(h) ^ st[i].triggerL;
        h = rotl1(h) ^ st[i].triggerR;
        h = rotl1(h) ^ st[i].analogA;
        h = rotl1(h) ^ st[i].analogB;
        h = rotl1(h) ^ (uint32_t)(uint8_t)st[i].err;
    }
    return h;
}

static inline void prefill(PADStatus *st, uint8_t v) {
    uint8_t *p = (uint8_t *)st;
    for (uint32_t i = 0; i < sizeof(PADStatus) * 4u; i++) p[i] = v;
}

static inline void dump_status(uint8_t *out, uint32_t *off, const PADStatus *st) {
    for (int i = 0; i < 4; i++) {
        wr32be(out + (*off) * 4u, st[i].button); (*off)++;
        wr32be(out + (*off) * 4u, (uint32_t)(uint8_t)st[i].stickX); (*off)++;
        wr32be(out + (*off) * 4u, (uint32_t)(uint8_t)st[i].stickY); (*off)++;
        wr32be(out + (*off) * 4u, (uint32_t)(uint8_t)st[i].substickX); (*off)++;
        wr32be(out + (*off) * 4u, (uint32_t)(uint8_t)st[i].substickY); (*off)++;
        wr32be(out + (*off) * 4u, st[i].triggerL); (*off)++;
        wr32be(out + (*off) * 4u, st[i].triggerR); (*off)++;
        wr32be(out + (*off) * 4u, st[i].analogA); (*off)++;
        wr32be(out + (*off) * 4u, st[i].analogB); (*off)++;
        wr32be(out + (*off) * 4u, (uint32_t)(uint8_t)st[i].err); (*off)++;
    }
}

static uint32_t rng_state;
static uint32_t rng_next(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

enum {
    L0_CASES = 16,
    L1_CASES = 16,
    L2_CASES = 16,
    L3_CASES = 512,
    L4_CASES = 8,
    L5_CASES = 8,
};

const char *gc_scenario_label(void) { return "PADRead/pbt_001"; }
const char *gc_scenario_out_path(void) { return "../actual/pad_read_pbt_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    uint8_t *out = gc_ram_ptr(ram, 0x80300000u, 0x100);
    if (!out) die("gc_ram_ptr(out) failed");
    PADStatus *st = (PADStatus *)gc_ram_ptr(ram, 0x80301000u, sizeof(PADStatus) * 4u);
    if (!st) die("gc_ram_ptr(status) failed");
    uint32_t word_off = 0u;
    uint32_t total_cases = 0u;
    uint32_t master_hash = 0u;
    uint32_t l0_hash = 0u, l1_hash = 0u, l2_hash = 0u, l3_hash = 0u, l4_hash = 0u, l5_hash = 0u;

    for (uint32_t i = 0; i < L0_CASES; i++) {
        gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_READ_CALLS, 0u);
        prefill(st, (uint8_t)i);
        u32 ret = PADRead(st);
        uint32_t h = ret ^ hash_status_fields(st) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_READ_CALLS);
        l0_hash = rotl1(l0_hash) ^ h;
    }
    total_cases += L0_CASES;
    master_hash = rotl1(master_hash) ^ l0_hash;

    for (uint32_t i = 0; i < L1_CASES; i++) {
        gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_READ_CALLS, 0u);
        prefill(st, (uint8_t)(0xA0u + i));
        u32 r0 = PADRead(st);
        u32 r1 = PADRead(st);
        uint32_t h = (r0 ^ (r1 << 1)) ^ hash_status_fields(st) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_READ_CALLS);
        l1_hash = rotl1(l1_hash) ^ h;
    }
    total_cases += L1_CASES;
    master_hash = rotl1(master_hash) ^ l1_hash;

    for (uint32_t i = 0; i < L2_CASES; i++) {
        gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_READ_CALLS, i);
        prefill(st, (uint8_t)(0xF0u - i));
        (void)PADRead(st);
        uint32_t h = hash_status_fields(st) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_READ_CALLS);
        l2_hash = rotl1(l2_hash) ^ h;
    }
    total_cases += L2_CASES;
    master_hash = rotl1(master_hash) ^ l2_hash;

    rng_state = 0x5A1234E1u;
    for (uint32_t i = 0; i < L3_CASES; i++) {
        gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_READ_CALLS, rng_next());
        prefill(st, (uint8_t)rng_next());
        u32 ret = PADRead((rng_next() & 1u) ? st : 0);
        uint32_t h = ret ^ hash_status_fields(st) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_READ_CALLS);
        l3_hash = rotl1(l3_hash) ^ h;
    }
    total_cases += L3_CASES;
    master_hash = rotl1(master_hash) ^ l3_hash;

    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_READ_CALLS, 0u);
    prefill(st, 0xCCu);
    for (uint32_t i = 0; i < L4_CASES; i++) {
        u32 ret = PADRead(st);
        l4_hash = rotl1(l4_hash) ^ (ret ^ hash_status_fields(st) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_READ_CALLS));
    }
    total_cases += L4_CASES;
    master_hash = rotl1(master_hash) ^ l4_hash;

    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_READ_CALLS, 0u);
    prefill(st, 0x11u);
    for (uint32_t i = 0; i < L5_CASES; i++) {
        u32 ret = PADRead((i & 1u) ? 0 : st);
        l5_hash = rotl1(l5_hash) ^ (ret ^ hash_status_fields(st) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_READ_CALLS));
    }
    total_cases += L5_CASES;
    master_hash = rotl1(master_hash) ^ l5_hash;

    wr32be(out + word_off * 4u, 0x50425244u); word_off++;
    wr32be(out + word_off * 4u, total_cases); word_off++;
    wr32be(out + word_off * 4u, master_hash); word_off++;
    wr32be(out + word_off * 4u, l0_hash); word_off++;
    wr32be(out + word_off * 4u, L0_CASES); word_off++;
    wr32be(out + word_off * 4u, l1_hash); word_off++;
    wr32be(out + word_off * 4u, L1_CASES); word_off++;
    wr32be(out + word_off * 4u, l2_hash); word_off++;
    wr32be(out + word_off * 4u, L2_CASES); word_off++;
    wr32be(out + word_off * 4u, l3_hash); word_off++;
    wr32be(out + word_off * 4u, L3_CASES); word_off++;
    wr32be(out + word_off * 4u, l4_hash); word_off++;
    wr32be(out + word_off * 4u, L4_CASES); word_off++;
    wr32be(out + word_off * 4u, l5_hash); word_off++;
    wr32be(out + word_off * 4u, L5_CASES); word_off++;

    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_READ_CALLS, 0u);
    prefill(st, 0x00u);
    (void)PADRead(st);
    dump_status(out, &word_off, st);

    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_READ_CALLS, 0u);
    prefill(st, 0x7Fu);
    (void)PADRead(0);
    dump_status(out, &word_off, st);

    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_READ_CALLS, 10u);
    prefill(st, 0xFFu);
    (void)PADRead(st);
    dump_status(out, &word_off, st);
}
