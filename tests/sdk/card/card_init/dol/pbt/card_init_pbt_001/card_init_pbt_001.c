#include <stdint.h>

#include "src/sdk_port/gc_mem.c"

typedef uint32_t u32;
typedef int32_t s32;

enum { CARD_RESULT_NOCARD = -3 };

typedef struct GcCardControl {
    s32 result;
    u32 thread_queue_inited;
    u32 alarm_created;
    uintptr_t disk_id;
} GcCardControl;

void oracle_CARDInit(void);
extern GcCardControl gc_card_block[2];
extern u32 gc_card_dsp_init_calls;
extern u32 gc_card_os_init_alarm_calls;
extern u32 gc_card_os_register_reset_calls;

enum { L0 = 8, L1 = 16, L2 = 8, L3 = 2048, L4 = 6, L5 = 4 };

static inline void be(volatile uint8_t *p, u32 v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static inline u32 rotl1(u32 v) { return (v << 1) | (v >> 31); }

static inline void reset_state(u32 s) {
    gc_card_block[0].result = (s32)(0x11110000u ^ s);
    gc_card_block[1].result = (s32)(0x22220000u ^ (s * 3u));
    gc_card_block[0].thread_queue_inited = 0xAAAA0000u ^ s;
    gc_card_block[1].thread_queue_inited = 0xBBBB0000u ^ (s * 5u);
    gc_card_block[0].alarm_created = 0xCCCC0000u ^ s;
    gc_card_block[1].alarm_created = 0xDDDD0000u ^ (s * 7u);
    gc_card_block[0].disk_id = (s & 1u) ? (uintptr_t)0x80000000u : (uintptr_t)0;
    gc_card_block[1].disk_id = (s & 2u) ? (uintptr_t)0x80000000u : (uintptr_t)0;

    gc_card_dsp_init_calls = 0xE0000000u ^ s;
    gc_card_os_init_alarm_calls = 0xE1000000u ^ (s * 11u);
    gc_card_os_register_reset_calls = 0xE2000000u ^ (s * 13u);
}

static inline u32 h(void) {
    u32 x = 0;
    x = rotl1(x) ^ (u32)gc_card_block[0].disk_id;
    x = rotl1(x) ^ (u32)gc_card_block[1].disk_id;
    x = rotl1(x) ^ (u32)gc_card_block[0].result;
    x = rotl1(x) ^ (u32)gc_card_block[1].result;
    x = rotl1(x) ^ gc_card_block[0].thread_queue_inited;
    x = rotl1(x) ^ gc_card_block[1].thread_queue_inited;
    x = rotl1(x) ^ gc_card_block[0].alarm_created;
    x = rotl1(x) ^ gc_card_block[1].alarm_created;
    x = rotl1(x) ^ gc_card_dsp_init_calls;
    x = rotl1(x) ^ gc_card_os_init_alarm_calls;
    x = rotl1(x) ^ gc_card_os_register_reset_calls;
    return x;
}

static inline void dump_probe(volatile uint8_t *out, u32 *off) {
    be(out + (*off) * 4u, (u32)gc_card_block[0].disk_id); (*off)++;
    be(out + (*off) * 4u, (u32)gc_card_block[1].disk_id); (*off)++;
    be(out + (*off) * 4u, (u32)gc_card_block[0].result); (*off)++;
    be(out + (*off) * 4u, (u32)gc_card_block[1].result); (*off)++;
    be(out + (*off) * 4u, gc_card_block[0].thread_queue_inited); (*off)++;
    be(out + (*off) * 4u, gc_card_block[1].thread_queue_inited); (*off)++;
    be(out + (*off) * 4u, gc_card_block[0].alarm_created); (*off)++;
    be(out + (*off) * 4u, gc_card_block[1].alarm_created); (*off)++;
    be(out + (*off) * 4u, gc_card_dsp_init_calls); (*off)++;
    be(out + (*off) * 4u, gc_card_os_init_alarm_calls); (*off)++;
    be(out + (*off) * 4u, gc_card_os_register_reset_calls); (*off)++;
}

static u32 rs;
static inline u32 rn(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);

    volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
    u32 off = 0, tc = 0, m = 0, a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;

    reset_state(0u); oracle_CARDInit(); a = rotl1(a) ^ h();
    reset_state(1u); oracle_CARDInit(); a = rotl1(a) ^ h();
    reset_state(2u); oracle_CARDInit(); a = rotl1(a) ^ h();
    reset_state(3u); oracle_CARDInit(); a = rotl1(a) ^ h();

    reset_state(0u);
    oracle_CARDInit();
    oracle_CARDInit();
    a = rotl1(a) ^ h();

    reset_state(0u);
    gc_card_block[0].disk_id = (uintptr_t)0x12345678u;
    gc_card_block[1].disk_id = (uintptr_t)0x9ABCDEF0u;
    oracle_CARDInit();
    a = rotl1(a) ^ h();

    reset_state(0u);
    gc_card_block[0].disk_id = (uintptr_t)0x80000000u;
    gc_card_block[1].disk_id = (uintptr_t)0x80000000u;
    gc_card_dsp_init_calls = 99u;
    gc_card_os_init_alarm_calls = 98u;
    gc_card_os_register_reset_calls = 97u;
    oracle_CARDInit();
    a = rotl1(a) ^ h();

    reset_state(0u);
    gc_card_block[0].disk_id = (uintptr_t)0x80000000u;
    gc_card_block[1].disk_id = (uintptr_t)0;
    gc_card_dsp_init_calls = 0u;
    gc_card_os_init_alarm_calls = 0u;
    gc_card_os_register_reset_calls = 0u;
    oracle_CARDInit();
    a = rotl1(a) ^ h();

    tc += L0; m = rotl1(m) ^ a;

    for (u32 i = 0; i < L1; i++) {
        reset_state(0x100u + i);
        oracle_CARDInit();
        b = rotl1(b) ^ h();
    }
    tc += L1; m = rotl1(m) ^ b;

    for (u32 i = 0; i < L2; i++) {
        reset_state(0x200u + i);
        oracle_CARDInit();
        oracle_CARDInit();
        c = rotl1(c) ^ h();
    }
    tc += L2; m = rotl1(m) ^ c;

    rs = 0xC4D1A117u;
    for (u32 i = 0; i < L3; i++) {
        reset_state(rn());
        oracle_CARDInit();
        d = rotl1(d) ^ h();
    }
    tc += L3; m = rotl1(m) ^ d;

    reset_state(0x400u);
    oracle_CARDInit(); e = rotl1(e) ^ h();
    oracle_CARDInit(); e = rotl1(e) ^ h();
    oracle_CARDInit(); e = rotl1(e) ^ h();
    reset_state(0x401u); oracle_CARDInit(); e = rotl1(e) ^ h();
    reset_state(0x402u); oracle_CARDInit(); e = rotl1(e) ^ h();
    reset_state(0x403u); oracle_CARDInit(); e = rotl1(e) ^ h();
    tc += L4; m = rotl1(m) ^ e;

    reset_state(0x500u);
    gc_card_block[0].disk_id = (uintptr_t)0x80000000u;
    gc_card_block[1].disk_id = (uintptr_t)0x80000000u;
    gc_card_block[0].result = CARD_RESULT_NOCARD;
    gc_card_block[1].result = CARD_RESULT_NOCARD;
    gc_card_block[0].thread_queue_inited = 0x1111u;
    gc_card_block[1].thread_queue_inited = 0x2222u;
    gc_card_block[0].alarm_created = 0x3333u;
    gc_card_block[1].alarm_created = 0x4444u;
    gc_card_dsp_init_calls = 1u;
    gc_card_os_init_alarm_calls = 1u;
    gc_card_os_register_reset_calls = 1u;
    oracle_CARDInit(); f = rotl1(f) ^ h();
    oracle_CARDInit(); f = rotl1(f) ^ h();
    oracle_CARDInit(); f = rotl1(f) ^ h();
    oracle_CARDInit(); f = rotl1(f) ^ h();
    tc += L5; m = rotl1(m) ^ f;

    be(out + off * 4u, 0x4344494Eu); off++;
    be(out + off * 4u, tc); off++;
    be(out + off * 4u, m); off++;
    be(out + off * 4u, a); off++; be(out + off * 4u, L0); off++;
    be(out + off * 4u, b); off++; be(out + off * 4u, L1); off++;
    be(out + off * 4u, c); off++; be(out + off * 4u, L2); off++;
    be(out + off * 4u, d); off++; be(out + off * 4u, L3); off++;
    be(out + off * 4u, e); off++; be(out + off * 4u, L4); off++;
    be(out + off * 4u, f); off++; be(out + off * 4u, L5); off++;

    reset_state(0x600u); oracle_CARDInit(); dump_probe(out, &off);
    reset_state(0x601u); oracle_CARDInit(); dump_probe(out, &off);
    reset_state(0x602u); gc_card_block[0].disk_id = 0; gc_card_block[1].disk_id = 0; oracle_CARDInit(); dump_probe(out, &off);
    reset_state(0x603u); gc_card_block[0].disk_id = 0x80000000u; gc_card_block[1].disk_id = 0x80000000u; oracle_CARDInit(); dump_probe(out, &off);

    while (1) { __asm__ volatile("nop"); }
    return 0;
}
