#include <stdint.h>

// Minimal, host-buildable slice of MP4 decomp: Hu3DPreProc from hsfman.c.
//
// Purpose: reachability/integration only (NOT a correctness oracle).
// We intentionally avoid pulling the full 3D engine; we only model the state
// that Hu3DPreProc touches directly, and keep the rest stubbed.

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int16_t s16;

typedef struct {
    u8 r, g, b, a;
} GXColor;

// Only fields Hu3DPreProc reads/writes.
typedef struct {
    void *hsfData;
    u32 attr;
} ModelDataMini;

enum { HU3D_MODEL_MAX = 512 };
enum { HU3D_ATTR_MOT_EXEC = 0x800 };

// Minimal global state used by Hu3DPreProc.
static ModelDataMini Hu3DData[HU3D_MODEL_MAX];
static GXColor BGColor;

static u32 totalPolyCnted, totalMatCnted, totalTexCnted, totalTexCacheCnted;
static u32 totalPolyCnt, totalMatCnt, totalTexCnt, totalTexCacheCnt;

// sdk_port
void GXSetCopyClear(GXColor clear_clr, u32 clear_z);

void Hu3DPreProc(void) {
    ModelDataMini *data;
    s16 i;

    GXSetCopyClear(BGColor, 0x00FFFFFFu);
    data = &Hu3DData[0];
    for (i = 0; i < HU3D_MODEL_MAX; i++, data++) {
        if (data->hsfData != 0) {
            data->attr &= ~HU3D_ATTR_MOT_EXEC;
        }
    }
    totalPolyCnted = totalPolyCnt;
    totalMatCnted = totalMatCnt;
    totalTexCnted = totalTexCnt;
    totalTexCacheCnted = totalTexCacheCnt;
    totalPolyCnt = totalMatCnt = totalTexCnt = totalTexCacheCnt = 0;
}

