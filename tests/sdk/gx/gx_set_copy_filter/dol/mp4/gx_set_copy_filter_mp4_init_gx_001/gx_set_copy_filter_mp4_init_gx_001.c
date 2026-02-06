#include <stdint.h>
typedef uint32_t u32;
typedef uint8_t u8;

static u32 hash_bytes(const void *p, u32 n) {
    const u8 *b = (const u8 *)p;
    u32 h = 2166136261u;
    u32 i;
    for (i = 0; i < n; i++) {
        h ^= (u32)b[i];
        h *= 16777619u;
    }
    return h;
}

static volatile u32 s_aa;
static volatile u32 s_vf;
static volatile u32 s_sample_hash;
static volatile u32 s_vfilter_hash;

static void GXSetCopyFilter(u8 aa, const u8 sample_pattern[12][2], u8 vf, const u8 vfilter[7]) {
    s_aa = aa;
    s_vf = vf;
    s_sample_hash = hash_bytes(sample_pattern, 12u * 2u);
    s_vfilter_hash = hash_bytes(vfilter, 7u);
}

int main(void) {
    // MP4 init.c calls:
    //   GXSetCopyFilter(RenderMode->aa, RenderMode->sample_pattern, GX_TRUE, RenderMode->vfilter)
    static const u8 sample_pattern[12][2] = {
        {6, 6}, {6, 6}, {6, 6}, {6, 6},
        {6, 6}, {6, 6}, {6, 6}, {6, 6},
        {6, 6}, {6, 6}, {6, 6}, {6, 6},
    };
    static const u8 vfilter[7] = {0, 0, 21, 22, 21, 0, 0};

    GXSetCopyFilter(0, sample_pattern, 1, vfilter);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_aa;
    *(volatile u32 *)0x80300008 = s_vf;
    *(volatile u32 *)0x8030000C = s_sample_hash;
    *(volatile u32 *)0x80300010 = s_vfilter_hash;
    while (1) {}
}
