#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void GXSetTevColorIn(uint32_t stage, uint32_t a, uint32_t b, uint32_t c, uint32_t d);
void GXSetTevAlphaIn(uint32_t stage, uint32_t a, uint32_t b, uint32_t c, uint32_t d);
void GXSetAlphaCompare(uint32_t comp0, uint8_t ref0, uint32_t op, uint32_t comp1, uint8_t ref1);
void GXSetAlphaUpdate(uint8_t update_enable);

extern uint32_t gc_gx_tevc[16];
extern uint32_t gc_gx_teva[16];
extern uint32_t gc_gx_cmode0;
extern uint32_t gc_gx_last_ras_reg;

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

    for (uint32_t i = 0; i < iters; i++) {
        uint32_t stage = xs32(&seed) % 20u; // includes out-of-range
        uint32_t a = xs32(&seed) & 0xFu, b = xs32(&seed) & 0xFu, c = xs32(&seed) & 0xFu, d = xs32(&seed) & 0xFu;
        uint32_t tevc_before = (stage < 16u) ? gc_gx_tevc[stage] : 0;
        GXSetTevColorIn(stage, a, b, c, d);
        if (stage < 16u) {
            uint32_t exp = tevc_before;
            exp = set_field(exp, 4, 12, a);
            exp = set_field(exp, 4, 8, b);
            exp = set_field(exp, 4, 4, c);
            exp = set_field(exp, 4, 0, d);
            if (gc_gx_tevc[stage] != exp || gc_gx_last_ras_reg != exp) {
                fprintf(stderr, "PBT FAIL: tev color pack mismatch\n");
                return 1;
            }
        }

        uint32_t teva_before = (stage < 16u) ? gc_gx_teva[stage] : 0;
        GXSetTevAlphaIn(stage, a & 7u, b & 7u, c & 7u, d & 7u);
        if (stage < 16u) {
            uint32_t exp = teva_before;
            exp = set_field(exp, 3, 13, a & 7u);
            exp = set_field(exp, 3, 10, b & 7u);
            exp = set_field(exp, 3, 7, c & 7u);
            exp = set_field(exp, 3, 4, d & 7u);
            if (gc_gx_teva[stage] != exp || gc_gx_last_ras_reg != exp) {
                fprintf(stderr, "PBT FAIL: tev alpha pack mismatch\n");
                return 1;
            }
        }

        uint32_t comp0 = xs32(&seed), comp1 = xs32(&seed), op = xs32(&seed);
        uint8_t ref0 = (uint8_t)xs32(&seed), ref1 = (uint8_t)xs32(&seed);
        GXSetAlphaCompare(comp0, ref0, op, comp1, ref1);
        uint32_t exp_ac = 0;
        exp_ac = set_field(exp_ac, 8, 0, (uint32_t)ref0);
        exp_ac = set_field(exp_ac, 8, 8, (uint32_t)ref1);
        exp_ac = set_field(exp_ac, 3, 16, comp0 & 7u);
        exp_ac = set_field(exp_ac, 3, 19, comp1 & 7u);
        exp_ac = set_field(exp_ac, 2, 22, op & 3u);
        exp_ac = set_field(exp_ac, 8, 24, 0xF3u);
        if (gc_gx_last_ras_reg != exp_ac) {
            fprintf(stderr, "PBT FAIL: alpha compare pack mismatch\n");
            return 1;
        }

        uint32_t cm_before = gc_gx_cmode0;
        uint8_t en = (uint8_t)(xs32(&seed) & 1u);
        GXSetAlphaUpdate(en);
        uint32_t exp_cm = set_field(cm_before, 1, 4, en ? 1u : 0u);
        if (gc_gx_cmode0 != exp_cm || gc_gx_last_ras_reg != exp_cm) {
            fprintf(stderr, "PBT FAIL: alpha update mismatch\n");
            return 1;
        }
    }

    printf("PBT PASS: gx_alpha_tev_packing %u iterations\n", iters);
    return 0;
}
