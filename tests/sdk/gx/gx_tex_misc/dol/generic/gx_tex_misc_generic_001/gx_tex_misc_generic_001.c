#include <stdint.h>
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;
typedef int8_t   s8;

/* gc_mem init */
#include "src/sdk_port/gc_mem.h"
#include "src/sdk_port/sdk_state.h"

/* Struct layouts matching GX.c internals. */
typedef struct {
    u32 image1;
    u32 image2;
    u16 sizeEven;
    u16 sizeOdd;
    u8  is32bMipmap;
    u8  isCached;
    u8  _pad[2];
} GXTexRegion;

typedef struct {
    u32 tlut;
    u32 loadTlut0;
    u16 numEntries;
    u16 _pad;
} GXTlutObj;

typedef struct {
    u32 loadTlut1;
    GXTlutObj tlutObj;
} GXTlutRegion;

/* Functions under test. */
void GXPixModeSync(void);
void GXSetTexCoordScaleManually(u32 coord, u8 enable, u16 ss, u16 ts);
void GXInitTexCacheRegion(GXTexRegion *region, u8 is_32b_mipmap,
                          u32 tmem_even, u32 size_even,
                          u32 tmem_odd, u32 size_odd);
void GXInitTlutRegion(GXTlutRegion *region, u32 tmem_addr, u8 tlut_sz);

/* Observable state. */
extern u32 gc_gx_pe_ctrl;
extern u32 gc_gx_bp_sent_not;
extern u32 gc_gx_tcs_man_enab;
extern u32 gc_gx_su_ts0[8];
extern u32 gc_gx_su_ts1[8];

static inline void wr32be(volatile uint8_t *p, u32 v) {
    p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16);
    p[2]=(uint8_t)(v>>8);  p[3]=(uint8_t)(v);
}

/* GX_TEXCACHE_32K=0, GX_TEXCACHE_128K=1, GX_TEXCACHE_512K=2, GX_TEXCACHE_NONE=3 */

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);
    gc_sdk_state_reset();

    volatile uint8_t *out = (volatile uint8_t *)0x80300000;
    for (u32 i = 0; i < 0xC0; i++) out[i] = 0;

    u32 off = 0;

    /* --- T1: GXPixModeSync --- */
    gc_gx_bp_sent_not = 1;
    GXPixModeSync();
    wr32be(out + off, 0x7E40A001u); off += 4;
    wr32be(out + off, gc_gx_bp_sent_not); off += 4;

    /* --- T2: GXSetTexCoordScaleManually — enable coord 2 --- */
    GXSetTexCoordScaleManually(2, 1, 256, 128);
    wr32be(out + off, 0x7E40A002u); off += 4;
    wr32be(out + off, gc_gx_tcs_man_enab); off += 4;
    wr32be(out + off, gc_gx_su_ts0[2]); off += 4;
    wr32be(out + off, gc_gx_su_ts1[2]); off += 4;

    /* --- T3: GXSetTexCoordScaleManually — enable coord 5 --- */
    GXSetTexCoordScaleManually(5, 1, 512, 64);
    wr32be(out + off, 0x7E40A003u); off += 4;
    wr32be(out + off, gc_gx_tcs_man_enab); off += 4;
    wr32be(out + off, gc_gx_su_ts0[5]); off += 4;
    wr32be(out + off, gc_gx_su_ts1[5]); off += 4;

    /* --- T4: GXSetTexCoordScaleManually — disable coord 2 --- */
    GXSetTexCoordScaleManually(2, 0, 0, 0);
    wr32be(out + off, 0x7E40A004u); off += 4;
    wr32be(out + off, gc_gx_tcs_man_enab); off += 4;

    /* --- T5: GXInitTexCacheRegion — 128K even, 32K odd --- */
    {
        GXTexRegion rgn;
        __builtin_memset(&rgn, 0, sizeof(rgn));
        GXInitTexCacheRegion(&rgn, 0, 0x00000, 1, 0x08000, 0);
        wr32be(out + off, 0x7E40A005u); off += 4;
        wr32be(out + off, rgn.image1); off += 4;
        wr32be(out + off, rgn.image2); off += 4;
        wr32be(out + off, (u32)rgn.is32bMipmap); off += 4;
        wr32be(out + off, (u32)rgn.isCached); off += 4;
    }

    /* --- T6: GXInitTexCacheRegion — 512K even, NONE odd, 32b mipmap --- */
    {
        GXTexRegion rgn;
        __builtin_memset(&rgn, 0, sizeof(rgn));
        GXInitTexCacheRegion(&rgn, 1, 0x10000, 2, 0x00000, 3);
        wr32be(out + off, 0x7E40A006u); off += 4;
        wr32be(out + off, rgn.image1); off += 4;
        wr32be(out + off, rgn.image2); off += 4;
        wr32be(out + off, (u32)rgn.is32bMipmap); off += 4;
        wr32be(out + off, (u32)rgn.isCached); off += 4;
    }

    /* --- T7: GXInitTlutRegion — addr=0xC0000, size=4 --- */
    {
        GXTlutRegion rgn;
        __builtin_memset(&rgn, 0, sizeof(rgn));
        GXInitTlutRegion(&rgn, 0xC0000, 4);
        wr32be(out + off, 0x7E40A007u); off += 4;
        wr32be(out + off, rgn.loadTlut1); off += 4;
    }

    /* --- T8: GXInitTlutRegion — addr=0, size=0 --- */
    {
        GXTlutRegion rgn;
        __builtin_memset(&rgn, 0, sizeof(rgn));
        GXInitTlutRegion(&rgn, 0, 0);
        wr32be(out + off, 0x7E40A008u); off += 4;
        wr32be(out + off, rgn.loadTlut1); off += 4;
    }

    while (1) { __asm__ volatile("nop"); }
    return 0;
}
