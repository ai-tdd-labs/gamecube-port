#pragma once

#include "dolphin/types.h"

// Minimal type surface for compiling MP4 GWInit slice.
// Only includes fields that GWInit directly touches.

typedef struct {
    s16 language;
} GameStat;

typedef struct {
    u8 _dummy;
} SystemState;

typedef struct {
    u8 _dummy;
} PlayerState;

typedef struct {
    s32 character;
    s32 pad_idx;
    s32 diff;
    s32 group;
    s32 iscom;
} PlayerConfig;

extern GameStat GWGameStatDefault;
extern GameStat GWGameStat;
extern SystemState GWSystem;
extern PlayerState GWPlayer[4];
extern PlayerConfig GWPlayerCfg[4];

