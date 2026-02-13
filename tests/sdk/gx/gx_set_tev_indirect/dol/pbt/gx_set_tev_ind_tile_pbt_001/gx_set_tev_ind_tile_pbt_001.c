#include <stdint.h>

#include "src/sdk_port/gc_mem.c"

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef int32_t s32;

typedef s32 GXTevStageID;
typedef s32 GXIndTexStageID;
typedef s32 GXIndTexFormat;
typedef s32 GXIndTexBiasSel;
typedef s32 GXIndTexMtxID;
typedef s32 GXIndTexAlphaSel;

void GXSetTevIndTile(GXTevStageID tev_stage, GXIndTexStageID ind_stage,
                     u16 tilesize_s, u16 tilesize_t,
                     u16 tilespacing_s, u16 tilespacing_t,
                     GXIndTexFormat format, GXIndTexMtxID matrix_sel,
                     GXIndTexBiasSel bias_sel, GXIndTexAlphaSel alpha_sel);

extern u32 gc_gx_tev_ind[16];
extern u32 gc_gx_ind_mtx[9];
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
    for (u32 i = 0; i < 16u; i++) gc_gx_tev_ind[i] = s ^ (0x01010101u * (i + 1u));
    for (u32 i = 0; i < 9u; i++) gc_gx_ind_mtx[i] = (s << (i & 7u)) ^ (0x11110000u + i);
    gc_gx_bp_sent_not = s ^ 0xA5A5A5A5u;
}

static inline u32 hash_state(void) {
    u32 h = 0u;
    for (u32 i = 0; i < 16u; i++) h = rotl1(h) ^ gc_gx_tev_ind[i];
    for (u32 i = 0; i < 9u; i++) h = rotl1(h) ^ gc_gx_ind_mtx[i];
    h = rotl1(h) ^ gc_gx_bp_sent_not;
    return h;
}

static inline void dump_state(volatile uint8_t *out, u32 *off) {
    store_u32be_ptr(out + (*off) * 4u, gc_gx_tev_ind[0]); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_gx_tev_ind[3]); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_gx_tev_ind[15]); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_gx_ind_mtx[0]); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_gx_ind_mtx[4]); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_gx_ind_mtx[8]); (*off)++;
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
    L4_CASES = 4,
    L5_CASES = 4,
};

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);

    volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
    u32 word_off = 0u;
    u32 total_cases = 0u;
    u32 master_hash = 0u;
    u32 l0_hash = 0u, l1_hash = 0u, l2_hash = 0u, l3_hash = 0u, l4_hash = 0u, l5_hash = 0u;

    seed_state(0u); GXSetTevIndTile(0, 0, 256, 256, 128, 64, 0, 1, 7, 0); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(1u); GXSetTevIndTile(1, 1, 128, 64, 64, 128, 1, 2, 3, 1); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(2u); GXSetTevIndTile(2, 2, 64, 32, 512, 256, 2, 3, 5, 2); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(3u); GXSetTevIndTile(3, 3, 16, 16, 16, 16, 3, 9, 0, 0); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(4u); GXSetTevIndTile(4, 0, 30, 128, 32, 48, 0, 1, 7, 0); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(5u); GXSetTevIndTile(5, 0, 256, 12, 4096, 1024, 0, 2, 7, 3); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(6u); GXSetTevIndTile(15, 1, 32, 64, 2048, 2048, 1, 10, 1, 0); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(7u); GXSetTevIndTile(16, 1, 64, 64, 128, 128, 2, 2, 7, 2); l0_hash = rotl1(l0_hash) ^ hash_state();
    total_cases += L0_CASES;
    master_hash = rotl1(master_hash) ^ l0_hash;

    for (u32 i = 0; i < 4u; i++) {
        for (u32 j = 0; j < 4u; j++) {
            seed_state(0x100u + (i << 4) + j);
            u16 sizes[4] = {16, 32, 64, 128};
            GXSetTevIndTile((GXTevStageID)(i * 4u + j), (GXIndTexStageID)(j & 3u),
                            sizes[i], sizes[j], (u16)(64u << i), (u16)(32u << j),
                            (GXIndTexFormat)((i + j) & 3u), (GXIndTexMtxID)(1u + ((i + j) % 3u)),
                            (GXIndTexBiasSel)((i * 2u + j) & 7u), (GXIndTexAlphaSel)((i ^ j) & 3u));
            l1_hash = rotl1(l1_hash) ^ hash_state();
        }
    }
    total_cases += L1_CASES;
    master_hash = rotl1(master_hash) ^ l1_hash;

    for (u32 i = 0; i < 4u; i++) {
        seed_state(0x200u + i);
        GXSetTevIndTile((GXTevStageID)(i + 2u), (GXIndTexStageID)i, 64, 32, 128, 64,
                        0, 2, 7, 0);
        GXSetTevIndTile((GXTevStageID)(i + 2u), (GXIndTexStageID)i, 64, 32, 128, 64,
                        0, 2, 7, 0);
        l2_hash = rotl1(l2_hash) ^ hash_state();
    }
    total_cases += L2_CASES;
    master_hash = rotl1(master_hash) ^ l2_hash;

    rng_state = 0x1F2E3D4Cu;
    for (u32 i = 0; i < L3_CASES; i++) {
        seed_state(rng_next());
        GXSetTevIndTile((GXTevStageID)(rng_next() & 31u), (GXIndTexStageID)(rng_next() & 3u),
                        (u16)(1u << (4u + (rng_next() & 3u))),
                        (u16)(1u << (4u + (rng_next() & 3u))),
                        (u16)(rng_next() & 0x1FFFu), (u16)(rng_next() & 0x1FFFu),
                        (GXIndTexFormat)(rng_next() & 3u), (GXIndTexMtxID)(rng_next() & 15u),
                        (GXIndTexBiasSel)(rng_next() & 7u), (GXIndTexAlphaSel)(rng_next() & 3u));
        l3_hash = rotl1(l3_hash) ^ hash_state();
    }
    total_cases += L3_CASES;
    master_hash = rotl1(master_hash) ^ l3_hash;

    seed_state(0u);
    GXSetTevIndTile(3, 0, 64, 32, 128, 64, 0, 2, 7, 0);
    l4_hash = rotl1(l4_hash) ^ hash_state();
    GXSetTevIndTile(0, 0, 64, 32, 128, 64, 0, 2, 7, 0);
    l4_hash = rotl1(l4_hash) ^ hash_state();
    GXSetTevIndTile(8, 1, 128, 64, 256, 128, 1, 1, 3, 1);
    l4_hash = rotl1(l4_hash) ^ hash_state();
    GXSetTevIndTile(12, 2, 256, 16, 512, 64, 2, 3, 5, 2);
    l4_hash = rotl1(l4_hash) ^ hash_state();
    total_cases += L4_CASES;
    master_hash = rotl1(master_hash) ^ l4_hash;

    seed_state(0x33333333u);
    u32 pre = gc_gx_tev_ind[15];
    GXSetTevIndTile(16, 0, 64, 64, 64, 64, 0, 1, 7, 0);
    l5_hash = rotl1(l5_hash) ^ (pre ^ gc_gx_tev_ind[15] ^ gc_gx_ind_mtx[0]);

    seed_state(0x44444444u);
    GXSetTevIndTile(15, 0, 1, 2, 128, 64, 0, 1, 7, 0);
    l5_hash = rotl1(l5_hash) ^ hash_state();

    seed_state(0x55555555u);
    GXSetTevIndTile(15, 0, 64, 64, 0, 0, 0, 11, 7, 0);
    l5_hash = rotl1(l5_hash) ^ hash_state();

    seed_state(0x66666666u);
    GXSetTevIndTile(15, 0, 16, 256, 65535, 65535, 3, 0, 7, 3);
    l5_hash = rotl1(l5_hash) ^ hash_state();

    total_cases += L5_CASES;
    master_hash = rotl1(master_hash) ^ l5_hash;

    store_u32be_ptr(out + word_off * 4u, 0x47534954u); word_off++; // GSIT
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

    seed_state(0u); GXSetTevIndTile(3, 0, 64, 32, 128, 64, 0, 2, 7, 0); dump_state(out, &word_off);
    seed_state(0u); GXSetTevIndTile(15, 1, 32, 64, 2048, 2048, 1, 10, 1, 0); dump_state(out, &word_off);
    seed_state(0u); GXSetTevIndTile(16, 1, 64, 64, 128, 128, 2, 2, 7, 2); dump_state(out, &word_off);
    seed_state(0u); GXSetTevIndTile(4, 0, 30, 12, 32, 48, 0, 1, 7, 0); dump_state(out, &word_off);

    while (1) {
        __asm__ volatile("nop");
    }
    return 0;
}
