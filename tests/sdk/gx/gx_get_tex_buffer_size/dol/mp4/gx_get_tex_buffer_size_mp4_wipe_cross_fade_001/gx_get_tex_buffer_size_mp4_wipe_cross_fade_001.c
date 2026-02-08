#include <stdint.h>

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

// Minimal tile-shift logic from decomp_mario_party_4/src/dolphin/gx/GXTexture.c
static void __GXGetTexTileShift(u32 format, u32 *tileShiftX, u32 *tileShiftY) {
    switch (format & 0x3F) {
    case 0x4: // GX_TF_RGB565
        *tileShiftX = 2;
        *tileShiftY = 2;
        return;
    default:
        // Evidence-driven: extend formats as needed.
        *tileShiftX = 0;
        *tileShiftY = 0;
        return;
    }
}

static inline void gc_disable_interrupts(void) {
    u32 msr;
    __asm__ volatile("mfmsr %0" : "=r"(msr));
    msr &= ~0x8000u; // MSR[EE]
    __asm__ volatile("mtmsr %0; isync" : : "r"(msr));
}

static inline void gc_arm_decrementer_far_future(void) {
    __asm__ volatile(
        "lis 3,0x7fff\n"
        "ori 3,3,0xffff\n"
        "mtdec 3\n"
        :
        :
        : "r3");
}

static inline void gc_safepoint(void) {
    gc_disable_interrupts();
    gc_arm_decrementer_far_future();
}

static u32 GXGetTexBufferSize(u16 width, u16 height, u32 format, u8 mipmap, u8 max_lod) {
    u32 tileShiftX, tileShiftY;
    u32 tileBytes;
    u32 bufferSize;
    u32 nx;
    u32 ny;
    u32 level;

    __GXGetTexTileShift(format, &tileShiftX, &tileShiftY);

    // Only needed for MP4 RGB565 (not RGBA8/Z24X8).
    tileBytes = 32;

    if (mipmap == 1) {
        bufferSize = 0;
        for (level = 0; level < (u32)max_lod; level++) {
            nx = (width + (1u << tileShiftX) - 1u) >> tileShiftX;
            ny = (height + (1u << tileShiftY) - 1u) >> tileShiftY;
            bufferSize += tileBytes * (nx * ny);
            if (width == 1 && height == 1) {
                break;
            }
            width = (width > 1) ? (u16)(width >> 1) : 1;
            height = (height > 1) ? (u16)(height >> 1) : 1;
        }
    } else {
        nx = (width + (1u << tileShiftX) - 1u) >> tileShiftX;
        ny = (height + (1u << tileShiftY) - 1u) >> tileShiftY;
        bufferSize = nx * ny * tileBytes;
    }
    return bufferSize;
}

int main(void) {
    gc_safepoint();

    // From wipe.c: WipeInit sets w/h from GXRenderModeObj (fbWidth=640, efbHeight=480),
    // and WipeCrossFade uses GX_TF_RGB565, mipmap=GX_FALSE, max_lod=0.
    const u16 w = 640;
    const u16 h = 480;
    const u32 fmt = 0x4; // GX_TF_RGB565
    const u8 mipmap = 0;
    const u8 max_lod = 0;

    u32 size = GXGetTexBufferSize(w, h, fmt, mipmap, max_lod);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = size;
    // Pad to the standard 0x40-byte dump window so host/PPC comparisons are stable.
    u32 off;
    for (off = 0x08; off < 0x40; off += 4) {
        *(volatile u32 *)(0x80300000u + off) = 0;
    }

    while (1) gc_safepoint();
}
