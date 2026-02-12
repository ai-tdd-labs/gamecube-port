#pragma once

#include <stdint.h>

typedef struct {
    uint32_t state;
} TgPadMotorRng;

static inline uint32_t tg_pad_motor_xorshift32(TgPadMotorRng *rng) {
    uint32_t x = rng->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng->state = x;
    return x;
}

static inline void tg_pad_motor_seed(TgPadMotorRng *rng, uint32_t seed) {
    rng->state = seed ? seed : 0xC0DE1234u;
}

static inline void tg_pad_motor_pick_case(TgPadMotorRng *rng, uint32_t *chan, uint32_t *cmd) {
    static const uint8_t k_boundary[6][2] = {
        {0, 0}, {0, 2}, {3, 0}, {3, 2}, {1, 1}, {2, 1},
    };

    const uint32_t roll = tg_pad_motor_xorshift32(rng) % 1000u;
    if (roll < 700u) {
        // 70% trace-biased: mostly observed retail pair (chan=0, cmd=2).
        *chan = ((tg_pad_motor_xorshift32(rng) % 100u) < 85u) ? 0u : (tg_pad_motor_xorshift32(rng) % 4u);
        *cmd = ((tg_pad_motor_xorshift32(rng) % 100u) < 85u) ? 2u : (tg_pad_motor_xorshift32(rng) % 3u);
        return;
    }

    if (roll < 900u) {
        // 20% boundary-valid.
        const uint32_t i = tg_pad_motor_xorshift32(rng) % 6u;
        *chan = (uint32_t)k_boundary[i][0];
        *cmd = (uint32_t)k_boundary[i][1];
        return;
    }

    // 10% exploratory-valid.
    *chan = tg_pad_motor_xorshift32(rng) % 4u;
    *cmd = tg_pad_motor_xorshift32(rng) % 3u;
}
