#pragma once

#include "dolphin/types.h"

// Minimal MTX header surface for MP4 workload compilation.
// Uses the "C_" functions we port in src/sdk_port/mtx.

typedef f32 Mtx[3][4];
typedef f32 Mtx44[4][4];

void C_MTXIdentity(Mtx mtx);
void C_MTXOrtho(Mtx44 m, f32 t, f32 b, f32 l, f32 r, f32 n, f32 f);

// Common SDK aliases used by MP4 decomp.
#define MTXIdentity C_MTXIdentity
#define MTXOrtho C_MTXOrtho

