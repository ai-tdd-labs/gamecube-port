#include <stdint.h>

// Minimal mtx port for host-side deterministic tests.
//
// Source of truth: decomp_mario_party_4/src/dolphin/mtx/mtx.c
// We keep the same observable outputs (float matrix values) and avoid
// introducing any host-specific behavior.

typedef float f32;
typedef f32 Mtx[3][4];

void C_MTXIdentity(Mtx mtx) {
    mtx[0][0] = 1.0f;
    mtx[0][1] = 0.0f;
    mtx[0][2] = 0.0f;
    mtx[0][3] = 0.0f;

    mtx[1][0] = 0.0f;
    mtx[1][1] = 1.0f;
    mtx[1][2] = 0.0f;
    mtx[1][3] = 0.0f;

    mtx[2][0] = 0.0f;
    mtx[2][1] = 0.0f;
    mtx[2][2] = 1.0f;
    mtx[2][3] = 0.0f;
}

// The SDK exposes PSMTXIdentity as well; keep it identical for callers that
// use the paired-single versions in decomp code.
void PSMTXIdentity(Mtx m) { C_MTXIdentity(m); }

