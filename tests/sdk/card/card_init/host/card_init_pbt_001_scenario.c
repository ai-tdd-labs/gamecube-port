#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef int32_t s32;

enum { CARD_RESULT_NOCARD = -3 };

typedef struct GcCardControl {
    s32 result;
    u32 thread_queue_inited;
    u32 alarm_created;
    uintptr_t disk_id;
} GcCardControl;

void CARDInit(void);
extern GcCardControl gc_card_block[2];
extern u32 gc_card_dsp_init_calls;
extern u32 gc_card_os_init_alarm_calls;
extern u32 gc_card_os_register_reset_calls;

enum { L0 = 8, L1 = 16, L2 = 8, L3 = 2048, L4 = 6, L5 = 4 };

static inline u32 rotl1(u32 v) { return (v << 1) | (v >> 31); }

static inline void reset_state(u32 s) {
    /* Poison state so the test proves CARDInit writes what we think it writes. */
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

static inline void dump_probe(uint8_t *out, u32 *off) {
    wr32be(out + (*off) * 4u, (u32)gc_card_block[0].disk_id); (*off)++;
    wr32be(out + (*off) * 4u, (u32)gc_card_block[1].disk_id); (*off)++;
    wr32be(out + (*off) * 4u, (u32)gc_card_block[0].result); (*off)++;
    wr32be(out + (*off) * 4u, (u32)gc_card_block[1].result); (*off)++;
    wr32be(out + (*off) * 4u, gc_card_block[0].thread_queue_inited); (*off)++;
    wr32be(out + (*off) * 4u, gc_card_block[1].thread_queue_inited); (*off)++;
    wr32be(out + (*off) * 4u, gc_card_block[0].alarm_created); (*off)++;
    wr32be(out + (*off) * 4u, gc_card_block[1].alarm_created); (*off)++;
    wr32be(out + (*off) * 4u, gc_card_dsp_init_calls); (*off)++;
    wr32be(out + (*off) * 4u, gc_card_os_init_alarm_calls); (*off)++;
    wr32be(out + (*off) * 4u, gc_card_os_register_reset_calls); (*off)++;
}

static u32 rs;
static inline u32 rn(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

const char *gc_scenario_label(void) { return "CARDInit/pbt_001"; }
const char *gc_scenario_out_path(void) { return "../actual/card_init_pbt_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    uint8_t *out = gc_ram_ptr(ram, 0x80300000u, 0x100u);
    if (!out) die("gc_ram_ptr failed");

    u32 off = 0, tc = 0, m = 0, a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;

    /* L0: init/early-return matrix. */
    reset_state(0u);
    /* both disk_id = 0 => init */
    CARDInit();
    a = rotl1(a) ^ h();

    reset_state(1u);
    /* disk0 set, disk1 clear => init */
    CARDInit();
    a = rotl1(a) ^ h();

    reset_state(2u);
    /* disk0 clear, disk1 set => init */
    CARDInit();
    a = rotl1(a) ^ h();

    reset_state(3u);
    /* both set => early return */
    CARDInit();
    a = rotl1(a) ^ h();

    reset_state(0u);
    CARDInit();
    /* after init, second init should be a no-op */
    CARDInit();
    a = rotl1(a) ^ h();

    reset_state(0u);
    gc_card_block[0].disk_id = (uintptr_t)0x12345678u;
    gc_card_block[1].disk_id = (uintptr_t)0x9ABCDEF0u;
    CARDInit();
    a = rotl1(a) ^ h();

    reset_state(0u);
    gc_card_block[0].disk_id = (uintptr_t)0x80000000u;
    gc_card_block[1].disk_id = (uintptr_t)0x80000000u;
    gc_card_dsp_init_calls = 99u;
    gc_card_os_init_alarm_calls = 98u;
    gc_card_os_register_reset_calls = 97u;
    CARDInit();
    a = rotl1(a) ^ h();

    reset_state(0u);
    gc_card_block[0].disk_id = (uintptr_t)0x80000000u;
    gc_card_block[1].disk_id = (uintptr_t)0;
    gc_card_dsp_init_calls = 0u;
    gc_card_os_init_alarm_calls = 0u;
    gc_card_os_register_reset_calls = 0u;
    CARDInit();
    a = rotl1(a) ^ h();

    tc += L0; m = rotl1(m) ^ a;

    /* L1: accumulation over deterministic resets. */
    for (u32 i = 0; i < L1; i++) {
        reset_state(0x100u + i);
        CARDInit();
        b = rotl1(b) ^ h();
    }
    tc += L1; m = rotl1(m) ^ b;

    /* L2: idempotency sequences. */
    for (u32 i = 0; i < L2; i++) {
        reset_state(0x200u + i);
        CARDInit();
        CARDInit();
        c = rotl1(c) ^ h();
    }
    tc += L2; m = rotl1(m) ^ c;

    /* L3: seeded pseudo-random resets. */
    rs = 0xC4D1A117u;
    for (u32 i = 0; i < L3; i++) {
        reset_state(rn());
        CARDInit();
        d = rotl1(d) ^ h();
    }
    tc += L3; m = rotl1(m) ^ d;

    /* L4: callsite-style: init once, then no-op on repeat. */
    reset_state(0x400u);
    CARDInit(); e = rotl1(e) ^ h();
    CARDInit(); e = rotl1(e) ^ h();
    CARDInit(); e = rotl1(e) ^ h();
    reset_state(0x401u); CARDInit(); e = rotl1(e) ^ h();
    reset_state(0x402u); CARDInit(); e = rotl1(e) ^ h();
    reset_state(0x403u); CARDInit(); e = rotl1(e) ^ h();
    tc += L4; m = rotl1(m) ^ e;

    /* L5: boundary/no-op invariants on already-inited state. */
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
    CARDInit(); f = rotl1(f) ^ h();
    CARDInit(); f = rotl1(f) ^ h();
    CARDInit(); f = rotl1(f) ^ h();
    CARDInit(); f = rotl1(f) ^ h();
    tc += L5; m = rotl1(m) ^ f;

    wr32be(out + off * 4u, 0x4344494Eu); off++; /* CDIN */
    wr32be(out + off * 4u, tc); off++;
    wr32be(out + off * 4u, m); off++;
    wr32be(out + off * 4u, a); off++; wr32be(out + off * 4u, L0); off++;
    wr32be(out + off * 4u, b); off++; wr32be(out + off * 4u, L1); off++;
    wr32be(out + off * 4u, c); off++; wr32be(out + off * 4u, L2); off++;
    wr32be(out + off * 4u, d); off++; wr32be(out + off * 4u, L3); off++;
    wr32be(out + off * 4u, e); off++; wr32be(out + off * 4u, L4); off++;
    wr32be(out + off * 4u, f); off++; wr32be(out + off * 4u, L5); off++;

    reset_state(0x600u); CARDInit(); dump_probe(out, &off);
    reset_state(0x601u); CARDInit(); dump_probe(out, &off);
    reset_state(0x602u); gc_card_block[0].disk_id = 0; gc_card_block[1].disk_id = 0; CARDInit(); dump_probe(out, &off);
    reset_state(0x603u); gc_card_block[0].disk_id = 0x80000000u; gc_card_block[1].disk_id = 0x80000000u; CARDInit(); dump_probe(out, &off);
}
