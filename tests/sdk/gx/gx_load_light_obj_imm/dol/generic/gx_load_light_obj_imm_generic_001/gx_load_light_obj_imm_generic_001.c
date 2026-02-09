#include <stdint.h>
typedef uint32_t u32;

typedef struct { uint8_t r,g,b,a; } GXColor;

typedef struct {
  u32 reserved[3];
  u32 Color;
  float a[3];
  float k[3];
  float lpos[3];
  float ldir[3];
} GXLightObj;

void GXInitLightColor(GXLightObj *lt_obj, GXColor color);
void GXInitLightPos(GXLightObj *lt_obj, float x, float y, float z);
void GXLoadLightObjImm(GXLightObj *lt_obj, u32 light);

extern u32 gc_gx_light_loaded_mask;
extern GXLightObj gc_gx_light_loaded[8];

static inline void wr32be(volatile uint8_t *p, u32 v) {
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)(v >> 0);
}
static inline u32 fbits(float f) { u32 u; __builtin_memcpy(&u, &f, 4); return u; }

int main(void) {
  GXLightObj obj;
  __builtin_memset(&obj, 0, sizeof(obj));
  GXInitLightColor(&obj, (GXColor){.r=1,.g=2,.b=3,.a=4});
  GXInitLightPos(&obj, 1.0f, 2.0f, 3.0f);
  GXLoadLightObjImm(&obj, 1u); // GX_LIGHT0

  volatile uint8_t *out = (volatile uint8_t *)0x80300000;
  for (u32 i = 0; i < 0x40; i++) out[i] = 0;
  wr32be(out + 0x00, 0xDEADBEEFu);
  wr32be(out + 0x04, gc_gx_light_loaded_mask);
  wr32be(out + 0x08, gc_gx_light_loaded[0].Color);
  wr32be(out + 0x0C, fbits(gc_gx_light_loaded[0].lpos[0]));
  wr32be(out + 0x10, fbits(gc_gx_light_loaded[0].lpos[1]));
  wr32be(out + 0x14, fbits(gc_gx_light_loaded[0].lpos[2]));
  while (1) {}
}
