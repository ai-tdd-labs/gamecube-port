#include <stdint.h>

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef struct {
    u32 mode0;
    u32 mode1;
    u32 image0;
    u32 image3;
    u32 userData;
    u32 fmt;
    u32 tlutName;
    u16 loadCnt;
    u8 loadFmt;
    u8 flags;
} GXTexObj;

static inline u32 set_field(u32 r, u32 n, u32 s, u32 v) {
    u32 m = ((1u << n) - 1u) << s;
    return (r & ~m) | ((v << s) & m);
}

static inline u32 set_reg_macro(u32 r, u32 tag) {
    return (r & 0xF8FFFFFFu) | tag;
}

void oracle_GXInitTexObjCI(
    GXTexObj *obj,
    void *image_ptr,
    u16 width,
    u16 height,
    u32 format,
    u32 wrap_s,
    u32 wrap_t,
    u8 mipmap,
    u32 tlut_name
) {
    if (!obj) return;

    {
        u32 *w = (u32 *)(void *)obj;
        for (u32 i = 0; i < 8u; i++) {
            w[i] = 0;
        }
    }

    obj->mode0 = set_field(obj->mode0, 2, 0, wrap_s);
    obj->mode0 = set_field(obj->mode0, 2, 2, wrap_t);
    obj->mode0 = set_field(obj->mode0, 1, 4, 1u);

    if (mipmap != 0) {
        obj->flags |= 1u;
        obj->mode0 = set_reg_macro(obj->mode0, 0xC0u);
    } else {
        obj->mode0 = set_reg_macro(obj->mode0, 0x80u);
    }

    obj->fmt = format;

    obj->image0 = set_field(obj->image0, 10, 0, (u32)(width - 1u));
    obj->image0 = set_field(obj->image0, 10, 10, (u32)(height - 1u));
    obj->image0 = set_field(obj->image0, 4, 20, format & 0xFu);
    obj->image3 = set_field(obj->image3, 21, 0, (((u32)(uintptr_t)image_ptr) >> 5) & 0x01FFFFFFu);

    {
        u16 rowT = 2;
        u16 colT = 2;
        switch (format & 0xFu) {
            case 0:
            case 8:
                obj->loadFmt = 1;
                rowT = 3;
                colT = 3;
                break;
            case 1:
            case 2:
            case 9:
                obj->loadFmt = 2;
                rowT = 3;
                colT = 2;
                break;
            case 3:
            case 4:
            case 5:
            case 10:
                obj->loadFmt = 2;
                rowT = 2;
                colT = 2;
                break;
            case 6:
                obj->loadFmt = 3;
                rowT = 2;
                colT = 2;
                break;
            case 14:
                obj->loadFmt = 0;
                rowT = 3;
                colT = 3;
                break;
            default:
                obj->loadFmt = 2;
                rowT = 2;
                colT = 2;
                break;
        }

        {
            u32 rowC = ((u32)width + (1u << rowT) - 1u) >> rowT;
            u32 colC = ((u32)height + (1u << colT) - 1u) >> colT;
            obj->loadCnt = (u16)((rowC * colC) & 0x7FFFu);
        }
    }

    obj->flags |= 2u;
    obj->flags &= (u8)~2u;
    obj->tlutName = tlut_name;
}
