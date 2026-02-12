#include <stdint.h>

#include "src/sdk_port/gc_mem.c"
#include "src/sdk_port/sdk_state.h"

typedef uint16_t u16;
typedef uint8_t u8;
typedef int8_t s8;

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
} PADStatus;

void PADClamp(PADStatus *status);

static inline void store_u32be_ptr(volatile uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v >> 0);
}

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

static uint32_t rng_state;
static uint32_t rng_next(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static inline int8_t rng_s8(void) {
    return (int8_t)(rng_next() & 0xFFu);
}

static inline void seed_case(PADStatus *st, uint32_t i, uint32_t mode) {
    for (int c = 0; c < 4; c++) {
        st[c].button = (u16)(0xA000u | (u16)(i + c + mode));
        st[c].stickX = (s8)(rng_s8());
        st[c].stickY = (s8)(rng_s8());
        st[c].substickX = (s8)(rng_s8());
        st[c].substickY = (s8)(rng_s8());
        st[c].triggerL = (u8)rng_next();
        st[c].triggerR = (u8)rng_next();
        st[c].analogA = (u8)(0x20u + c);
        st[c].analogB = (u8)(0x40u + c);
        st[c].err = (mode == 2) ? (s8)-1 : (s8)((c == 0 && (i & 1u)) ? -1 : 0);
    }
}

static inline void dump_status(volatile uint8_t *out, uint32_t *off, const PADStatus *st) {
    for (int i = 0; i < 4; i++) {
        store_u32be_ptr(out + (*off) * 4u, st[i].button); (*off)++;
        store_u32be_ptr(out + (*off) * 4u, (uint32_t)(uint8_t)st[i].stickX); (*off)++;
        store_u32be_ptr(out + (*off) * 4u, (uint32_t)(uint8_t)st[i].stickY); (*off)++;
        store_u32be_ptr(out + (*off) * 4u, (uint32_t)(uint8_t)st[i].substickX); (*off)++;
        store_u32be_ptr(out + (*off) * 4u, (uint32_t)(uint8_t)st[i].substickY); (*off)++;
        store_u32be_ptr(out + (*off) * 4u, st[i].triggerL); (*off)++;
        store_u32be_ptr(out + (*off) * 4u, st[i].triggerR); (*off)++;
        store_u32be_ptr(out + (*off) * 4u, st[i].analogA); (*off)++;
        store_u32be_ptr(out + (*off) * 4u, st[i].analogB); (*off)++;
        store_u32be_ptr(out + (*off) * 4u, (uint32_t)(uint8_t)st[i].err); (*off)++;
    }
}

enum {
    L0_CASES = 24,
    L1_CASES = 24,
    L2_CASES = 24,
    L3_CASES = 512,
    L4_CASES = 64,
    L5_CASES = 16,
};

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);
    gc_sdk_state_reset();

    volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
    PADStatus *st = (PADStatus *)0x80301000u;
    uint32_t word_off = 0u;
    uint32_t total_cases = 0u;
    uint32_t master_hash = 0u;
    uint32_t l0_hash = 0u, l1_hash = 0u, l2_hash = 0u, l3_hash = 0u, l4_hash = 0u, l5_hash = 0u;

    rng_state = 0xC1A04E21u;
    for (uint32_t i = 0; i < L0_CASES; i++) {
        gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_CLAMP_CALLS, 0u);
        prefill(st, (uint8_t)i);
        seed_case(st, i, 0);
        PADClamp(st);
        l0_hash = rotl1(l0_hash) ^ (hash_status_fields(st) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_CLAMP_CALLS));
    }
    total_cases += L0_CASES;
    master_hash = rotl1(master_hash) ^ l0_hash;

    rng_state = 0x91B5DE02u;
    for (uint32_t i = 0; i < L1_CASES; i++) {
        gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_CLAMP_CALLS, 0u);
        prefill(st, 0xAAu);
        seed_case(st, i, 0);
        PADClamp(st);
        uint32_t h1 = hash_status_fields(st);
        PADClamp(st);
        uint32_t h2 = hash_status_fields(st);
        l1_hash = rotl1(l1_hash) ^ (h1 ^ (h2 << 1) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_CLAMP_CALLS));
    }
    total_cases += L1_CASES;
    master_hash = rotl1(master_hash) ^ l1_hash;

    rng_state = 0x7D23977Bu;
    for (uint32_t i = 0; i < L2_CASES; i++) {
        gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_CLAMP_CALLS, i);
        prefill(st, 0x66u);
        seed_case(st, i, 2); // all err != 0 should not clamp
        uint32_t before = hash_status_fields(st);
        PADClamp(st);
        uint32_t after = hash_status_fields(st);
        l2_hash = rotl1(l2_hash) ^ (before ^ (after << 1) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_CLAMP_CALLS));
    }
    total_cases += L2_CASES;
    master_hash = rotl1(master_hash) ^ l2_hash;

    rng_state = 0x1234ACE1u;
    for (uint32_t i = 0; i < L3_CASES; i++) {
        gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_CLAMP_CALLS, rng_next());
        prefill(st, (uint8_t)rng_next());
        seed_case(st, i, (rng_next() & 1u) ? 0u : 2u);
        PADClamp(st);
        l3_hash = rotl1(l3_hash) ^ (hash_status_fields(st) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_CLAMP_CALLS));
    }
    total_cases += L3_CASES;
    master_hash = rotl1(master_hash) ^ l3_hash;

    rng_state = 0x55AACC11u;
    for (uint32_t i = 0; i < L4_CASES; i++) {
        gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_CLAMP_CALLS, 0u);
        prefill(st, 0x00u);
        for (int c = 0; c < 4; c++) {
            st[c].button = 0u;
            st[c].stickX = (s8)(((int)(i * 3 + c * 7) % 145) - 72);
            st[c].stickY = (s8)(((int)(i * 5 + c * 11) % 145) - 72);
            st[c].substickX = (s8)(((int)(i * 2 + c * 13) % 119) - 59);
            st[c].substickY = (s8)(((int)(i * 4 + c * 17) % 119) - 59);
            st[c].triggerL = (u8)((i * 9 + c * 23) & 0xFFu);
            st[c].triggerR = (u8)((i * 7 + c * 19) & 0xFFu);
            st[c].analogA = (u8)c;
            st[c].analogB = (u8)(c + 1);
            st[c].err = 0;
        }
        PADClamp(st);
        l4_hash = rotl1(l4_hash) ^ (hash_status_fields(st) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_CLAMP_CALLS));
    }
    total_cases += L4_CASES;
    master_hash = rotl1(master_hash) ^ l4_hash;

    for (uint32_t i = 0; i < L5_CASES; i++) {
        gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_CLAMP_CALLS, 0u);
        prefill(st, 0x00u);
        for (int c = 0; c < 4; c++) {
            st[c].button = 0u;
            st[c].stickX = (s8)((i & 1u) ? 127 : -128);
            st[c].stickY = (s8)((i & 2u) ? 127 : -128);
            st[c].substickX = (s8)((i & 4u) ? 127 : -128);
            st[c].substickY = (s8)((i & 8u) ? 127 : -128);
            st[c].triggerL = (u8)((i & 1u) ? 255u : 0u);
            st[c].triggerR = (u8)((i & 2u) ? 255u : 0u);
            st[c].analogA = 0;
            st[c].analogB = 0;
            st[c].err = (c == 0) ? 0 : -1;
        }
        PADClamp(st);
        l5_hash = rotl1(l5_hash) ^ (hash_status_fields(st) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_CLAMP_CALLS));
    }
    total_cases += L5_CASES;
    master_hash = rotl1(master_hash) ^ l5_hash;

    store_u32be_ptr(out + word_off * 4u, 0x50434C4Du); word_off++; // PCLM
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

    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_CLAMP_CALLS, 0u);
    prefill(st, 0x00u);
    st[0].err = 0; st[0].stickX = 100; st[0].stickY = -100; st[0].triggerL = 200; st[0].triggerR = 31;
    st[1].err = -1; st[1].stickX = -90; st[1].stickY = 90; st[1].triggerL = 10; st[1].triggerR = 220;
    st[2].err = 0; st[2].substickX = 100; st[2].substickY = 100; st[2].triggerL = 29; st[2].triggerR = 180;
    st[3].err = 0; st[3].stickX = 15; st[3].stickY = 15; st[3].substickX = -15; st[3].substickY = -15;
    PADClamp(st);
    dump_status(out, &word_off, st);
    store_u32be_ptr(out + word_off * 4u, gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_CLAMP_CALLS)); word_off++;

    while (1) {
        __asm__ volatile("nop");
    }
    return 0;
}
