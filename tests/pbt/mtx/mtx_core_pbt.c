#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef float f32;
typedef f32 Mtx[3][4];
typedef f32 Mtx44[4][4];

void C_MTXIdentity(Mtx mtx);
void PSMTXIdentity(Mtx mtx);
void C_MTXOrtho(Mtx44 m, f32 t, f32 b, f32 l, f32 r, f32 n, f32 f);

static uint32_t xs32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static float rand_f32_range(uint32_t *state, float lo, float hi) {
    float t = (float)(xs32(state) & 0x00FFFFFFu) / 16777215.0f;
    return lo + (hi - lo) * t;
}

static int feq(float a, float b) {
    float d = fabsf(a - b);
    float scale = fmaxf(1.0f, fmaxf(fabsf(a), fabsf(b)));
    return d <= (1e-5f * scale);
}

int main(int argc, char **argv) {
    uint32_t iters = 200000;
    uint32_t seed = 0xC0DEC0DEu;
    if (argc >= 2) iters = (uint32_t)strtoul(argv[1], 0, 0);
    if (argc >= 3) seed = (uint32_t)strtoul(argv[2], 0, 0);

    for (uint32_t i = 0; i < iters; i++) {
        Mtx m;
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 4; c++) {
                m[r][c] = rand_f32_range(&seed, -1000.0f, 1000.0f);
            }
        }
        C_MTXIdentity(m);
        if (!feq(m[0][0], 1.0f) || !feq(m[1][1], 1.0f) || !feq(m[2][2], 1.0f)) {
            fprintf(stderr, "PBT FAIL: C_MTXIdentity diagonal\n");
            return 1;
        }
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 4; c++) {
                if ((r == c && c < 3)) continue;
                if (!feq(m[r][c], 0.0f)) {
                    fprintf(stderr, "PBT FAIL: C_MTXIdentity off-diagonal r=%d c=%d got=%f\n", r, c, m[r][c]);
                    return 1;
                }
            }
        }

        PSMTXIdentity(m);
        if (!feq(m[0][0], 1.0f) || !feq(m[1][1], 1.0f) || !feq(m[2][2], 1.0f)) {
            fprintf(stderr, "PBT FAIL: PSMTXIdentity diagonal\n");
            return 1;
        }

        // Ensure non-degenerate input ranges for ortho.
        float l = rand_f32_range(&seed, -500.0f, 400.0f);
        float r = l + rand_f32_range(&seed, 0.01f, 500.0f);
        float b = rand_f32_range(&seed, -500.0f, 400.0f);
        float t = b + rand_f32_range(&seed, 0.01f, 500.0f);
        float n = rand_f32_range(&seed, 0.01f, 100.0f);
        float f = n + rand_f32_range(&seed, 0.01f, 1000.0f);

        Mtx44 o;
        C_MTXOrtho(o, t, b, l, r, n, f);

        float m00 = 2.0f / (r - l);
        float m11 = 2.0f / (t - b);
        float m22 = -1.0f / (f - n);
        float m03 = -(r + l) / (r - l);
        float m13 = -(t + b) / (t - b);
        float m23 = -f / (f - n);
        if (!feq(o[0][0], m00) || !feq(o[1][1], m11) || !feq(o[2][2], m22)) {
            fprintf(stderr, "PBT FAIL: C_MTXOrtho diagonal mismatch\n");
            return 1;
        }
        if (!feq(o[0][3], m03) || !feq(o[1][3], m13) || !feq(o[2][3], m23)) {
            fprintf(stderr, "PBT FAIL: C_MTXOrtho translation mismatch\n");
            return 1;
        }
        if (!feq(o[3][3], 1.0f)) {
            fprintf(stderr, "PBT FAIL: C_MTXOrtho bottom-right\n");
            return 1;
        }
    }

    printf("PBT PASS: mtx_core %u iterations\n", iters);
    return 0;
}
