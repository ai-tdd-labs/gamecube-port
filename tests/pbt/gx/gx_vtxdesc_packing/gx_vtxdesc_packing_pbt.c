#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void GXSetVtxDesc(uint32_t attr, uint32_t type);
void GXSetVtxAttrFmt(uint32_t vtxfmt, uint32_t attr, uint32_t cnt, uint32_t type, uint8_t frac);

extern uint32_t gc_gx_vcd_lo;
extern uint32_t gc_gx_vcd_hi;
extern uint32_t gc_gx_has_nrms;
extern uint32_t gc_gx_has_binrms;
extern uint32_t gc_gx_nrm_type;
extern uint32_t gc_gx_dirty_state;
extern uint32_t gc_gx_dirty_vat;

static uint32_t xs32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static uint32_t set_field(uint32_t reg, uint32_t size, uint32_t shift, uint32_t v) {
    const uint32_t mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

int main(int argc, char **argv) {
    uint32_t iters = 200000;
    uint32_t seed = 0xC0DEC0DEu;
    if (argc >= 2) iters = (uint32_t)strtoul(argv[1], 0, 0);
    if (argc >= 3) seed = (uint32_t)strtoul(argv[2], 0, 0);

    enum { GX_VA_NRM = 10, GX_VA_NBT = 25 };
    for (uint32_t i = 0; i < iters; i++) {
        uint32_t attr = xs32(&seed) % 32u;
        uint32_t type = xs32(&seed) & 3u;
        uint32_t lo_before = gc_gx_vcd_lo;
        uint32_t hi_before = gc_gx_vcd_hi;

        GXSetVtxDesc(attr, type);
        if ((gc_gx_dirty_state & 8u) == 0u) {
            fprintf(stderr, "PBT FAIL: dirty_state bit8 not set\n");
            return 1;
        }

        // Spot-check known packed attrs.
        if (attr == 9u) { // GX_VA_POS
            uint32_t exp = set_field(lo_before, 2, 9, type);
            if ((gc_gx_vcd_lo & (3u << 9)) != (exp & (3u << 9))) {
                fprintf(stderr, "PBT FAIL: GX_VA_POS packing mismatch\n");
                return 1;
            }
        }
        if (attr >= 13u && attr <= 20u) { // GX_VA_TEX0..7
            uint32_t sh = (attr - 13u) * 2u;
            uint32_t exp = set_field(hi_before, 2, sh, type);
            if ((gc_gx_vcd_hi & (3u << sh)) != (exp & (3u << sh))) {
                fprintf(stderr, "PBT FAIL: GX_VA_TEXn packing mismatch\n");
                return 1;
            }
        }
        if (attr == GX_VA_NRM || attr == GX_VA_NBT) {
            if (type == 0u) {
                if (attr == GX_VA_NRM && gc_gx_has_nrms != 0u) return 1;
                if (attr == GX_VA_NBT && gc_gx_has_binrms != 0u) return 1;
            } else {
                if (gc_gx_nrm_type != type) {
                    fprintf(stderr, "PBT FAIL: nrm type mismatch\n");
                    return 1;
                }
            }
        }

        // VAT touch check.
        uint32_t vf = xs32(&seed) & 7u;
        GXSetVtxAttrFmt(vf, 9u, xs32(&seed) & 1u, xs32(&seed) & 7u, (uint8_t)(xs32(&seed) & 31u));
        if ((gc_gx_dirty_state & 0x10u) == 0u || ((gc_gx_dirty_vat >> vf) & 1u) == 0u) {
            fprintf(stderr, "PBT FAIL: VAT dirty bits not set\n");
            return 1;
        }
    }

    printf("PBT PASS: gx_vtxdesc_packing %u iterations\n", iters);
    return 0;
}
