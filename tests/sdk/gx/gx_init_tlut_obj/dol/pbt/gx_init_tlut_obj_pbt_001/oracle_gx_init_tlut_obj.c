#include <stdint.h>

typedef uint32_t u32;
typedef uint16_t u16;

typedef struct {
    u32 tlut;
    u32 loadTlut0;
    u16 numEntries;
    u16 _pad;
} GXTlutObj;

static inline u32 set_field(u32 r, u32 n, u32 s, u32 v) {
    u32 m = ((1u << n) - 1u) << s;
    return (r & ~m) | ((v << s) & m);
}

void oracle_GXInitTlutObj(GXTlutObj *tlut_obj, void *lut, u32 fmt, u16 n_entries) {
    if (!tlut_obj) return;

    tlut_obj->tlut = 0;
    tlut_obj->tlut = set_field(tlut_obj->tlut, 2, 10, fmt);

    tlut_obj->loadTlut0 = 0;
    tlut_obj->loadTlut0 = set_field(
        tlut_obj->loadTlut0,
        21,
        0,
        ((u32)((uintptr_t)lut) & 0x3FFFFFFFu) >> 5
    );
    tlut_obj->loadTlut0 = set_field(tlut_obj->loadTlut0, 8, 24, 0x64u);
    tlut_obj->numEntries = n_entries;
}
