#include <stdint.h>

uint32_t gc_ai_regs[4];

static inline uint32_t get_play(void) { return gc_ai_regs[0] & 1u; }
static inline uint32_t get_rate(void) { return (gc_ai_regs[0] >> 1) & 1u; }
static inline uint8_t get_l(void) { return (uint8_t)gc_ai_regs[1]; }
static inline uint8_t get_r(void) { return (uint8_t)(gc_ai_regs[1] >> 8); }
static inline void set_l(uint8_t v) { gc_ai_regs[1] = (gc_ai_regs[1] & ~0xFFu) | ((uint32_t)v & 0xFFu); }
static inline void set_r(uint8_t v) { gc_ai_regs[1] = (gc_ai_regs[1] & ~0xFF00u) | (((uint32_t)v & 0xFFu) << 8); }

uint32_t oracle_AIGetStreamPlayState(void) { return get_play(); }
uint32_t oracle_AIGetStreamSampleRate(void) { return get_rate(); }
uint8_t oracle_AIGetStreamVolLeft(void) { return get_l(); }
uint8_t oracle_AIGetStreamVolRight(void) { return get_r(); }
void oracle_AISetStreamVolLeft(uint8_t v) { set_l(v); }
void oracle_AISetStreamVolRight(uint8_t v) { set_r(v); }

void oracle_AISetStreamPlayState(uint32_t state)
{
    if (state == oracle_AIGetStreamPlayState()) {
        return;
    }

    if ((oracle_AIGetStreamSampleRate() == 0u) && (state == 1u)) {
        uint8_t volRight = oracle_AIGetStreamVolRight();
        uint8_t volLeft = oracle_AIGetStreamVolLeft();
        oracle_AISetStreamVolRight(0);
        oracle_AISetStreamVolLeft(0);
        /* __AI_SRC_INIT() is a no-op in this port test. */
        gc_ai_regs[0] = (gc_ai_regs[0] & ~0x20u) | 0x20u;
        gc_ai_regs[0] = (gc_ai_regs[0] & ~1u) | 1u;
        oracle_AISetStreamVolLeft(volRight);
        oracle_AISetStreamVolRight(volLeft);
    } else {
        gc_ai_regs[0] = (gc_ai_regs[0] & ~1u) | (state & 1u);
    }
}

