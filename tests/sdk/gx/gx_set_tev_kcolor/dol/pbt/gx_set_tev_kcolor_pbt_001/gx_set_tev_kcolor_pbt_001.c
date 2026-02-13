#include <stdint.h>

#include "src/sdk_port/gc_mem.c"

typedef uint32_t u32;
typedef uint8_t u8;

typedef struct { u8 r, g, b, a; } GXColor;

void GXSetTevKColor(u32 id, GXColor color);
void GXSetTevKColorSel(u32 stage, u32 sel);
void GXSetTevKAlphaSel(u32 stage, u32 sel);

extern u32 gc_gx_tev_ksel[8];
extern u32 gc_gx_tev_kcolor_ra[4];
extern u32 gc_gx_tev_kcolor_bg[4];
extern u32 gc_gx_bp_sent_not;

static inline void store_u32be_ptr(volatile uint8_t *p, u32 v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v >> 0);
}

static inline u32 rotl1(u32 v) {
    return (v << 1) | (v >> 31);
}

static inline void seed_state(u32 s) {
    for (u32 i = 0; i < 8u; i++) gc_gx_tev_ksel[i] = s ^ (0x01010101u * (i + 1u));
    for (u32 i = 0; i < 4u; i++) gc_gx_tev_kcolor_ra[i] = (s << (i + 1u)) ^ (0x11110000u + i);
    for (u32 i = 0; i < 4u; i++) gc_gx_tev_kcolor_bg[i] = (s >> (i + 1u)) ^ (0x00001111u + i);
    gc_gx_bp_sent_not = s ^ 0xA5A5A5A5u;
}

static inline u32 hash_state(void) {
    u32 h = 0u;
    for (u32 i = 0; i < 4u; i++) {
        h = rotl1(h) ^ gc_gx_tev_kcolor_ra[i];
        h = rotl1(h) ^ gc_gx_tev_kcolor_bg[i];
    }
    for (u32 i = 0; i < 8u; i++) {
        h = rotl1(h) ^ gc_gx_tev_ksel[i];
    }
    h = rotl1(h) ^ gc_gx_bp_sent_not;
    return h;
}

static inline void dump_state(volatile uint8_t *out, u32 *off) {
    store_u32be_ptr(out + (*off) * 4u, gc_gx_tev_kcolor_ra[0]); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_gx_tev_kcolor_bg[0]); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_gx_tev_kcolor_ra[2]); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_gx_tev_kcolor_bg[2]); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_gx_tev_ksel[0]); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_gx_tev_ksel[1]); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_gx_tev_ksel[7]); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_gx_bp_sent_not); (*off)++;
}

static u32 rng_state;

static u32 rng_next(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

enum {
    L0_CASES = 8,
    L1_CASES = 16,
    L2_CASES = 4,
    L3_CASES = 1024,
    L4_CASES = 6,
    L5_CASES = 2,
};

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);

    volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
    u32 word_off = 0u;
    u32 total_cases = 0u;
    u32 master_hash = 0u;
    u32 l0_hash = 0u, l1_hash = 0u, l2_hash = 0u, l3_hash = 0u, l4_hash = 0u, l5_hash = 0u;

    seed_state(0x00000000u); GXSetTevKColor(0u, (GXColor){0x11, 0x22, 0x33, 0x44}); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(0x01010101u); GXSetTevKColor(3u, (GXColor){0xFF, 0x00, 0x80, 0x7F}); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(0x02020202u); GXSetTevKColor(4u, (GXColor){0x99, 0x88, 0x77, 0x66}); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(0x03030303u); GXSetTevKColorSel(0u, 12u); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(0x04040404u); GXSetTevKColorSel(15u, 31u); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(0x05050505u); GXSetTevKAlphaSel(0u, 5u); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(0x06060606u); GXSetTevKAlphaSel(15u, 31u); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(0x07070707u); GXSetTevKAlphaSel(16u, 7u); l0_hash = rotl1(l0_hash) ^ hash_state();
    total_cases += L0_CASES;
    master_hash = rotl1(master_hash) ^ l0_hash;

    for (u32 i = 0; i < 4u; i++) {
        for (u32 j = 0; j < 4u; j++) {
            seed_state(0x100u + (i << 4) + j);
            GXSetTevKColor(i, (GXColor){(u8)(0x10u * i + j), (u8)(0x20u + i), (u8)(0x40u + j), (u8)(0x80u - i)});
            GXSetTevKColorSel(i * 4u + j, (i + j) & 31u);
            GXSetTevKAlphaSel(j * 4u + i, ((i << 2) ^ j) & 31u);
            l1_hash = rotl1(l1_hash) ^ hash_state();
        }
    }
    total_cases += L1_CASES;
    master_hash = rotl1(master_hash) ^ l1_hash;

    for (u32 i = 0; i < 4u; i++) {
        seed_state(0x200u + i);
        GXSetTevKColor(i, (GXColor){(u8)(0xA0u + i), (u8)(0xB0u + i), (u8)(0xC0u + i), (u8)(0xD0u + i)});
        GXSetTevKColor(i, (GXColor){(u8)(0xA0u + i), (u8)(0xB0u + i), (u8)(0xC0u + i), (u8)(0xD0u + i)});
        GXSetTevKAlphaSel(i * 3u, i * 7u);
        GXSetTevKAlphaSel(i * 3u, i * 7u);
        l2_hash = rotl1(l2_hash) ^ hash_state();
    }
    total_cases += L2_CASES;
    master_hash = rotl1(master_hash) ^ l2_hash;

    rng_state = 0xA1B2C3D4u;
    for (u32 i = 0; i < L3_CASES; i++) {
        seed_state(rng_next());
        GXSetTevKColor(rng_next() & 7u, (GXColor){(u8)rng_next(), (u8)rng_next(), (u8)rng_next(), (u8)rng_next()});
        GXSetTevKColorSel(rng_next() & 31u, rng_next());
        GXSetTevKAlphaSel(rng_next() & 31u, rng_next());
        l3_hash = rotl1(l3_hash) ^ hash_state();
    }
    total_cases += L3_CASES;
    master_hash = rotl1(master_hash) ^ l3_hash;

    seed_state(0u);
    GXSetTevKColor(0u, (GXColor){0x11, 0x22, 0x33, 0x44});
    l4_hash = rotl1(l4_hash) ^ hash_state();
    GXSetTevKColor(2u, (GXColor){0xAA, 0xBB, 0xCC, 0xDD});
    l4_hash = rotl1(l4_hash) ^ hash_state();
    GXSetTevKColorSel(0u, 12u);
    l4_hash = rotl1(l4_hash) ^ hash_state();
    GXSetTevKColorSel(3u, 20u);
    l4_hash = rotl1(l4_hash) ^ hash_state();
    GXSetTevKAlphaSel(0u, 5u);
    l4_hash = rotl1(l4_hash) ^ hash_state();
    GXSetTevKAlphaSel(3u, 18u);
    l4_hash = rotl1(l4_hash) ^ hash_state();
    total_cases += L4_CASES;
    master_hash = rotl1(master_hash) ^ l4_hash;

    seed_state(0x33333333u);
    u32 pre = hash_state();
    GXSetTevKAlphaSel(16u, 31u);
    l5_hash = rotl1(l5_hash) ^ (pre ^ hash_state());
    seed_state(0x44444444u);
    pre = hash_state();
    GXSetTevKColorSel(16u, 31u);
    l5_hash = rotl1(l5_hash) ^ (pre ^ hash_state());
    total_cases += L5_CASES;
    master_hash = rotl1(master_hash) ^ l5_hash;

    store_u32be_ptr(out + word_off * 4u, 0x47534B43u); word_off++; // GSKC
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

    seed_state(0u);
    GXSetTevKColor(0u, (GXColor){0x11, 0x22, 0x33, 0x44});
    GXSetTevKColorSel(0u, 12u);
    GXSetTevKAlphaSel(0u, 5u);
    dump_state(out, &word_off);

    seed_state(0u);
    GXSetTevKColor(2u, (GXColor){0xAA, 0xBB, 0xCC, 0xDD});
    GXSetTevKColorSel(3u, 20u);
    GXSetTevKAlphaSel(3u, 18u);
    dump_state(out, &word_off);

    seed_state(0x77777777u);
    GXSetTevKColor(4u, (GXColor){0x99, 0x88, 0x77, 0x66});
    GXSetTevKColorSel(15u, 31u);
    GXSetTevKAlphaSel(15u, 31u);
    dump_state(out, &word_off);

    seed_state(0x55555555u);
    GXSetTevKColor(1u, (GXColor){0x01, 0x23, 0x45, 0x67});
    GXSetTevKColorSel(16u, 31u);
    GXSetTevKAlphaSel(16u, 31u);
    dump_state(out, &word_off);

    while (1) {
        __asm__ volatile("nop");
    }
    return 0;
}
