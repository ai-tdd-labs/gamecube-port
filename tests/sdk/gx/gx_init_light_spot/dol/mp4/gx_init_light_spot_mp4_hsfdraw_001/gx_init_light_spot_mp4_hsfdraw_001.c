#include <stdint.h>

typedef uint32_t u32;

typedef struct {
  uint8_t r, g, b, a;
} GXColor;

typedef struct {
  u32 reserved[3];
  u32 Color;
  float a[3];
  float k[3];
  float lpos[3];
  float ldir[3];
} GXLightObj;

typedef enum {
  GX_SP_OFF,
  GX_SP_FLAT,
  GX_SP_COS,
  GX_SP_COS2,
  GX_SP_SHARP,
  GX_SP_RING1,
  GX_SP_RING2,
} GXSpotFn;

void GXInitLightSpot(GXLightObj *lt_obj, float cutoff, GXSpotFn spot_func);

static inline void wr32be(volatile uint8_t *p, u32 v) {
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)(v >> 0);
}

static inline u32 fbits(float f) {
  u32 u;
  __builtin_memcpy(&u, &f, 4);
  return u;
}

int main(void) {
  // MP4 hsfdraw.c uses:
  //   GXInitLightSpot(&sp20, 20.0f, GX_SP_COS);
  GXLightObj obj;
  __builtin_memset(&obj, 0, sizeof(obj));

  GXInitLightSpot(&obj, 20.0f, GX_SP_COS);

  volatile uint8_t *out = (volatile uint8_t *)0x80300000;
  // Standard dump window is 0x40 bytes.
  for (u32 i = 0; i < 0x40; i++) out[i] = 0;
  wr32be(out + 0x00, 0xDEADBEEFu);
  wr32be(out + 0x04, fbits(obj.a[0]));
  wr32be(out + 0x08, fbits(obj.a[1]));
  wr32be(out + 0x0C, fbits(obj.a[2]));
  while (1) {}
}
