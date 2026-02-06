#include <stdint.h>
typedef uint32_t u32;
typedef uint16_t u16;

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
} GXRenderModeObj;

static void GXAdjustForOverscan(GXRenderModeObj *rmin, GXRenderModeObj *rmout, u16 hor, u16 ver) {
    u16 hor2 = (u16)(hor * 2u);
    u16 ver2 = (u16)(ver * 2u);
    u32 verf;

    if (rmin != rmout) *rmout = *rmin;

    rmout->fbWidth = (u16)(rmin->fbWidth - hor2);
    verf = (u32)(ver2 * rmin->efbHeight) / (u32)rmin->xfbHeight;
    rmout->efbHeight = (u16)(rmin->efbHeight - verf);

    if (rmin->xFBmode == 1u && ((rmin->viTVmode & 2u) != 2u)) {
        rmout->xfbHeight = (u16)(rmin->xfbHeight - ver);
    } else {
        rmout->xfbHeight = (u16)(rmin->xfbHeight - ver2);
    }

    rmout->viWidth = (u16)(rmin->viWidth - hor2);
    rmout->viHeight = (u16)(rmin->viHeight - ver2);
    rmout->viXOrigin = (u16)(rmin->viXOrigin + hor);
    rmout->viYOrigin = (u16)(rmin->viYOrigin + ver);
}

int main(void) {
    GXRenderModeObj in = {
        .viTVmode = 0,
        .fbWidth = 640,
        .efbHeight = 480,
        .xfbHeight = 480,
        .viXOrigin = 40,
        .viYOrigin = 0,
        .viWidth = 640,
        .viHeight = 480,
        .xFBmode = 1,
    };
    GXRenderModeObj out;

    GXAdjustForOverscan(&in, &out, 0, 16);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = out.fbWidth;
    *(volatile u32 *)0x80300008 = out.efbHeight;
    *(volatile u32 *)0x8030000C = out.xfbHeight;
    *(volatile u32 *)0x80300010 = out.viXOrigin;
    *(volatile u32 *)0x80300014 = out.viYOrigin;
    *(volatile u32 *)0x80300018 = out.viWidth;
    *(volatile u32 *)0x8030001C = out.viHeight;
    while (1) {}
}
