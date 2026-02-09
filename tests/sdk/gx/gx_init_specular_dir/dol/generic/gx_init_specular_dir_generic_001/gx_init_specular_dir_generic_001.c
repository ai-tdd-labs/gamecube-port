#include <stdint.h>
typedef uint32_t u32;

typedef struct {
  u32 reserved[3];
  u32 Color;
  float a[3];
  float k[3];
  float lpos[3];
  float ldir[3];
} GXLightObj;

void GXInitSpecularDir(GXLightObj *lt_obj, float nx, float ny, float nz);

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
  GXInitSpecularDir(&obj, 0.1f, 0.2f, 0.3f);

  volatile uint8_t *out = (volatile uint8_t *)0x80300000;
  for (u32 i = 0; i < 0x40; i++) out[i] = 0;
  wr32be(out + 0x00, 0xDEADBEEFu);
  // ldir[0..2]
  wr32be(out + 0x04, fbits(obj.ldir[0]));
  wr32be(out + 0x08, fbits(obj.ldir[1]));
  wr32be(out + 0x0C, fbits(obj.ldir[2]));
  // lpos[0..2]
  wr32be(out + 0x10, fbits(obj.lpos[0]));
  wr32be(out + 0x14, fbits(obj.lpos[1]));
  wr32be(out + 0x18, fbits(obj.lpos[2]));
  while (1) {}
}
