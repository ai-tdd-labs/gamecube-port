#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void GXSetTexCopyDst(uint16_t wd, uint16_t ht, uint32_t fmt, uint32_t mipmap);
uint32_t GXGetTexBufferSize(uint16_t width, uint16_t height, uint32_t format, uint8_t mipmap, uint8_t max_lod);

extern uint32_t gc_gx_cp_tex_stride;
extern uint32_t gc_gx_cp_tex;

static uint32_t xs32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

int main(int argc, char **argv) {
    uint32_t iters = 200000;
    uint32_t seed = 0xC0DEC0DEu;
    if (argc >= 2) iters = (uint32_t)strtoul(argv[1], 0, 0);
    if (argc >= 3) seed = (uint32_t)strtoul(argv[2], 0, 0);

    for (uint32_t i = 0; i < iters; i++) {
        uint16_t w = (uint16_t)((xs32(&seed) % 1024u) + 1u);
        uint16_t h = (uint16_t)((xs32(&seed) % 1024u) + 1u);
        uint32_t fmt = xs32(&seed) & 0x3Fu;
        uint32_t mip = xs32(&seed) & 1u;

        GXSetTexCopyDst(w, h, fmt, mip);
        uint32_t stride_tiles = gc_gx_cp_tex_stride & 0x3FFu;
        uint32_t exp_stride_tiles = ((uint32_t)w + 7u) / 8u; // get_image_tile_count in GX.c
        if (stride_tiles != exp_stride_tiles) {
            fprintf(stderr, "PBT FAIL: stride tiles got=%u exp=%u (w=%u)\n", stride_tiles, exp_stride_tiles, w);
            return 1;
        }
        if (((gc_gx_cp_tex >> 9) & 1u) != (mip ? 1u : 0u)) {
            fprintf(stderr, "PBT FAIL: mip flag mismatch\n");
            return 1;
        }
        if (((gc_gx_cp_tex >> 4) & 7u) != (fmt & 7u)) {
            fprintf(stderr, "PBT FAIL: fmt low bits mismatch\n");
            return 1;
        }
        if (((gc_gx_cp_tex >> 3) & 1u) != ((fmt >> 3) & 1u)) {
            fprintf(stderr, "PBT FAIL: fmt hi bit mismatch\n");
            return 1;
        }

        uint32_t s0 = GXGetTexBufferSize(w, h, fmt, 0, 8);
        uint32_t s1 = GXGetTexBufferSize(w, h, fmt, 1, 1);
        uint32_t s2 = GXGetTexBufferSize(w, h, fmt, 1, 2);
        if (s1 != s0) {
            fprintf(stderr, "PBT FAIL: mip lod1 must equal base size\n");
            return 1;
        }
        if (s2 < s1) {
            fprintf(stderr, "PBT FAIL: mip lod2 must be >= lod1\n");
            return 1;
        }

        // Cross-check lower bound relation between both APIs.
        uint32_t col_tiles = ((uint32_t)h + 3u) / 4u;
        uint32_t min_bytes = stride_tiles * col_tiles * 32u;
        if (s0 < min_bytes) {
            fprintf(stderr, "PBT FAIL: size lower bound mismatch s0=%u min=%u\n", s0, min_bytes);
            return 1;
        }
    }

    printf("PBT PASS: gx_texcopy_relation %u iterations\n", iters);
    return 0;
}
