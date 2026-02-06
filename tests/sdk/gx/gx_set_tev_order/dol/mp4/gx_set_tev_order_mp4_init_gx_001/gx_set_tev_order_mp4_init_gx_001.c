#include <stdint.h>

typedef uint32_t u32;
typedef uint8_t u8;

typedef struct { u8 _dummy; } GXFifoObj;
static GXFifoObj s_fifo_obj;

static u32 s_tref[8];
static u32 s_texmap_id[16];
static u32 s_tev_tc_enab;
static u32 s_dirty_state;
static u32 s_bp_sent_not;

static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

static void GXSetTevOrder(u32 stage, u32 coord, u32 map, u32 color) {
    static const int c2r[] = { 0, 1, 0, 1, 0, 1, 7, 5, 6 };
    u32 *ptref = &s_tref[stage / 2];
    s_texmap_id[stage] = map;

    u32 tmap = map & ~0x100u;
    if (tmap >= 8u) tmap = 0;

    u32 tcoord;
    if (coord >= 8u) {
        tcoord = 0;
        s_tev_tc_enab &= ~(1u << stage);
    } else {
        tcoord = coord;
        s_tev_tc_enab |= (1u << stage);
    }

    u32 rasc = (color == 0xFFu) ? 7u : (u32)c2r[color];
    u32 enable = (map != 0xFFu) && ((map & 0x100u) == 0);

    if (stage & 1u) {
        *ptref = set_field(*ptref, 3, 12, tmap);
        *ptref = set_field(*ptref, 3, 15, tcoord);
        *ptref = set_field(*ptref, 3, 19, rasc);
        *ptref = set_field(*ptref, 1, 18, enable);
    } else {
        *ptref = set_field(*ptref, 3, 0, tmap);
        *ptref = set_field(*ptref, 3, 3, tcoord);
        *ptref = set_field(*ptref, 3, 7, rasc);
        *ptref = set_field(*ptref, 1, 6, enable);
    }

    s_bp_sent_not = 0;
    s_dirty_state |= 1u;
}

static GXFifoObj *GXInit(void *base, u32 size) {
    (void)base; (void)size;
    u32 i;
    for (i = 0; i < 8; i++) s_tref[i] = 0;
    for (i = 0; i < 16; i++) s_texmap_id[i] = 0;
    s_tev_tc_enab = 0;
    s_dirty_state = 0;
    s_bp_sent_not = 1;
    return &s_fifo_obj;
}

enum { GX_TEVSTAGE0 = 0 };
enum { GX_TEXCOORD0 = 0 };
enum { GX_TEXMAP0 = 0, GX_TEXMAP_NULL = 0xFF };
enum { GX_COLOR0A0 = 4 };

int main(void) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_tref[0];
    *(volatile u32 *)0x80300008 = s_texmap_id[0];
    *(volatile u32 *)0x8030000C = s_tev_tc_enab;
    *(volatile u32 *)0x80300010 = s_dirty_state;
    *(volatile u32 *)0x80300014 = s_bp_sent_not;
    while (1) {}
}
