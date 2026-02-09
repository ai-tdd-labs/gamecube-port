#include <stdint.h>
#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

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

static inline u32 fbits(float f) { u32 u; __builtin_memcpy(&u, &f, 4); return u; }

const char *gc_scenario_label(void) { return "GXLoadLightObjImm/generic"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_load_light_obj_imm_generic_001.bin"; }

void gc_scenario_run(GcRam *ram) {
  GXLightObj obj;
  __builtin_memset(&obj, 0, sizeof(obj));
  GXInitLightColor(&obj, (GXColor){.r=1,.g=2,.b=3,.a=4});
  GXInitLightPos(&obj, 1.0f, 2.0f, 3.0f);
  GXLoadLightObjImm(&obj, 1u);

  uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x40);
  if (!p) die("gc_ram_ptr failed");
  for (u32 i = 0; i < 0x40; i += 4) wr32be(p + i, 0);
  wr32be(p + 0x00, 0xDEADBEEFu);
  wr32be(p + 0x04, gc_gx_light_loaded_mask);
  wr32be(p + 0x08, gc_gx_light_loaded[0].Color);
  wr32be(p + 0x0C, fbits(gc_gx_light_loaded[0].lpos[0]));
  wr32be(p + 0x10, fbits(gc_gx_light_loaded[0].lpos[1]));
  wr32be(p + 0x14, fbits(gc_gx_light_loaded[0].lpos[2]));
}
