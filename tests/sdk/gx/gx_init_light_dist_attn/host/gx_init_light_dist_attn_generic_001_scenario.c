#include <stdint.h>
#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;

typedef struct {
  u32 reserved[3];
  u32 Color;
  float a[3];
  float k[3];
  float lpos[3];
  float ldir[3];
} GXLightObj;

typedef enum { GX_DA_OFF, GX_DA_GENTLE, GX_DA_MEDIUM, GX_DA_STEEP } GXDistAttnFn;

void GXInitLightDistAttn(GXLightObj *lt_obj, float ref_dist, float ref_br, GXDistAttnFn dist_func);

static inline u32 fbits(float f) { u32 u; __builtin_memcpy(&u, &f, 4); return u; }

const char *gc_scenario_label(void) { return "GXInitLightDistAttn/generic"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_init_light_dist_attn_generic_001.bin"; }

void gc_scenario_run(GcRam *ram) {
  GXLightObj obj;
  __builtin_memset(&obj, 0, sizeof(obj));
  GXInitLightDistAttn(&obj, 100.0f, 0.5f, GX_DA_MEDIUM);

  uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x40);
  if (!p) die("gc_ram_ptr failed");
  for (u32 i = 0; i < 0x40; i += 4) wr32be(p + i, 0);
  wr32be(p + 0x00, 0xDEADBEEFu);
  wr32be(p + 0x04, fbits(obj.k[0]));
  wr32be(p + 0x08, fbits(obj.k[1]));
  wr32be(p + 0x0C, fbits(obj.k[2]));
}
