/*
 * gxoverscan_oracle.h --- Oracle for GXAdjustForOverscan property tests.
 *
 * Exact copy of decomp GXAdjustForOverscan with oracle_ prefix.
 *
 * Source of truth: external/mp4-decomp/src/dolphin/gx/GXFrameBuf.c
 */
#pragma once

#include <stdint.h>
#include <string.h>

typedef unsigned int u32;
typedef unsigned short u16;

typedef struct {
    u32 viTVmode;
    u16 fbWidth;
    u16 efbHeight;
    u16 xfbHeight;
    u16 viXOrigin;
    u16 viYOrigin;
    u16 viWidth;
    u16 viHeight;
    u32 xFBmode;
} oracle_GXRenderModeObj;

/* VI_XFBMODE_SF = 1 from decomp */
#define ORACLE_VI_XFBMODE_SF 1u

/* ── GXAdjustForOverscan (GXFrameBuf.c:81-107) ── */
static void oracle_GXAdjustForOverscan(oracle_GXRenderModeObj *rmin,
    oracle_GXRenderModeObj *rmout, u16 hor, u16 ver)
{
    u16 hor2 = (u16)(hor * 2);
    u16 ver2 = (u16)(ver * 2);
    u32 verf;

    if (rmin != rmout) {
        *rmout = *rmin;
    }

    rmout->fbWidth = (u16)(rmin->fbWidth - hor2);
    verf = (u32)(ver2 * rmin->efbHeight) / (u32)rmin->xfbHeight;
    rmout->efbHeight = (u16)(rmin->efbHeight - verf);
    if (rmin->xFBmode == ORACLE_VI_XFBMODE_SF && ((rmin->viTVmode & 2u) != 2u)) {
        rmout->xfbHeight = (u16)(rmin->xfbHeight - ver);
    } else {
        rmout->xfbHeight = (u16)(rmin->xfbHeight - ver2);
    }

    rmout->viWidth = (u16)(rmin->viWidth - hor2);
    rmout->viHeight = (u16)(rmin->viHeight - ver2);
    rmout->viXOrigin = (u16)(rmin->viXOrigin + hor);
    rmout->viYOrigin = (u16)(rmin->viYOrigin + ver);
}
