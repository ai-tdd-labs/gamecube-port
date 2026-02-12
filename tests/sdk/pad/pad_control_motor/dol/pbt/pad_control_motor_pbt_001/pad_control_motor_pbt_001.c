#include <stdint.h>

#include "src/sdk_port/gc_mem.c"
#include "src/sdk_port/sdk_state.h"
#include "src/sdk_port/pad/PAD.c"

static inline void store_u32be_ptr(volatile uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v >> 0);
}

static inline uint32_t rotl1(uint32_t v) {
    return (v << 1) | (v >> 31);
}

static inline uint32_t hash_slots(void) {
    uint32_t h = 0u;
    h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 0u);
    h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 4u);
    h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 8u);
    h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 12u);
    return h;
}

static inline void clear_slots(void) {
    for (uint32_t i = 0; i < 4u; i++) {
        gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + i * 4u, 0u);
    }
}

static inline void fill_slots(uint32_t v0, uint32_t v1, uint32_t v2, uint32_t v3) {
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 0u, v0);
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 4u, v1);
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 8u, v2);
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 12u, v3);
}

static inline void dump_slots(volatile uint8_t *out, uint32_t *off) {
    store_u32be_ptr(out + (*off) * 4u, gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 0u)); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 4u)); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 8u)); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 12u)); (*off)++;
}

static uint32_t rng_state;

static uint32_t rng_next(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static const int32_t k_l0_chans[] = {-2, -1, 0, 1, 2, 3, 4, 5};
static const uint32_t k_cmds_ext[] = {0u, 1u, 2u, 3u, 0xffffffffu};
static const uint32_t k_cmds_valid[] = {0u, 1u, 2u};
static const int32_t k_l4_harvest_chans[] = {0, 0, 0, 0};
static const uint32_t k_l4_harvest_cmds[] = {2u, 2u, 2u, 2u};

enum {
    L0_CASES = 8 * 5,
    L1_CASES = 4 * 4 * 3 * 3,
    L2_CASES = 4 * 3 * 3,
    L3_CASES = 1024,
    L4_CASES = 4,
    L5_CASES = 4 + 3 + 1,
};

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);
    gc_sdk_state_reset();

    volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
    uint32_t word_off = 0u;

    uint32_t total_cases = 0u;
    uint32_t master_hash = 0u;

    uint32_t l0_hash = 0u, l1_hash = 0u, l2_hash = 0u, l3_hash = 0u, l4_hash = 0u, l5_hash = 0u;

    // L0: isolated matrix.
    for (uint32_t ci = 0; ci < 8u; ci++) {
        for (uint32_t mi = 0; mi < 5u; mi++) {
            clear_slots();
            PADControlMotor(k_l0_chans[ci], k_cmds_ext[mi]);
            l0_hash = rotl1(l0_hash) ^ hash_slots();
        }
    }
    total_cases += L0_CASES;
    master_hash = rotl1(master_hash) ^ l0_hash;

    // L1: accumulation (two valid calls without reset).
    for (uint32_t c0 = 0; c0 < 4u; c0++) {
        for (uint32_t c1 = 0; c1 < 4u; c1++) {
            for (uint32_t m0 = 0; m0 < 3u; m0++) {
                for (uint32_t m1 = 0; m1 < 3u; m1++) {
                    clear_slots();
                    PADControlMotor((int32_t)c0, k_cmds_valid[m0]);
                    PADControlMotor((int32_t)c1, k_cmds_valid[m1]);
                    l1_hash = rotl1(l1_hash) ^ hash_slots();
                }
            }
        }
    }
    total_cases += L1_CASES;
    master_hash = rotl1(master_hash) ^ l1_hash;

    // L2: overwrite (same channel twice, second value must win).
    for (uint32_t ch = 0; ch < 4u; ch++) {
        for (uint32_t m0 = 0; m0 < 3u; m0++) {
            for (uint32_t m1 = 0; m1 < 3u; m1++) {
                clear_slots();
                PADControlMotor((int32_t)ch, k_cmds_valid[m0]);
                PADControlMotor((int32_t)ch, k_cmds_valid[m1]);
                l2_hash = rotl1(l2_hash) ^ hash_slots();
            }
        }
    }
    total_cases += L2_CASES;
    master_hash = rotl1(master_hash) ^ l2_hash;

    // L3: random-start state + mixed valid/invalid channels.
    rng_state = 0xC0DE1234u;
    for (uint32_t i = 0; i < L3_CASES; i++) {
        fill_slots(rng_next(), rng_next(), rng_next(), rng_next());
        int32_t ch = (int32_t)(rng_next() % 8u) - 2;
        uint32_t cmd = k_cmds_ext[rng_next() % 5u];
        PADControlMotor(ch, cmd);
        l3_hash = rotl1(l3_hash) ^ hash_slots();
    }
    total_cases += L3_CASES;
    master_hash = rotl1(master_hash) ^ l3_hash;

    // L4: harvest replay sequence.
    clear_slots();
    for (uint32_t i = 0; i < L4_CASES; i++) {
        PADControlMotor(k_l4_harvest_chans[i], k_l4_harvest_cmds[i]);
        l4_hash = rotl1(l4_hash) ^ hash_slots();
    }
    total_cases += L4_CASES;
    master_hash = rotl1(master_hash) ^ l4_hash;

    // L5: boundary + no-op invariants.
    fill_slots(0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u);
    for (uint32_t i = 0; i < 4u; i++) {
        PADControlMotor(k_l0_chans[i], 2u);
        l5_hash = rotl1(l5_hash) ^ hash_slots();
    }
    for (uint32_t i = 0; i < 3u; i++) {
        clear_slots();
        PADControlMotor(3, k_cmds_valid[i]);
        l5_hash = rotl1(l5_hash) ^ hash_slots();
    }
    clear_slots();
    PADControlMotor(0, 0xffffffffu);
    l5_hash = rotl1(l5_hash) ^ hash_slots();
    total_cases += L5_CASES;
    master_hash = rotl1(master_hash) ^ l5_hash;

    // Header + per-level hashes.
    store_u32be_ptr(out + word_off * 4u, 0x50425054u); word_off++; // PBPT
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

    // Spot states for fast debug.
    clear_slots();
    PADControlMotor(1, 2u);
    dump_slots(out, &word_off);

    clear_slots();
    PADControlMotor(0, 1u);
    PADControlMotor(3, 2u);
    dump_slots(out, &word_off);

    clear_slots();
    for (uint32_t i = 0; i < L4_CASES; i++) {
        PADControlMotor(k_l4_harvest_chans[i], k_l4_harvest_cmds[i]);
    }
    dump_slots(out, &word_off);

    fill_slots(0xaaaa5555u, 0xbbbb6666u, 0xcccc7777u, 0xdddd8888u);
    PADControlMotor(-1, 2u);
    dump_slots(out, &word_off);

    while (1) {
        __asm__ volatile("nop");
    }
    return 0;
}
