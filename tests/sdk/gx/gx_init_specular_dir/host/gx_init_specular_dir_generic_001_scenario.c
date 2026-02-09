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

void GXInitSpecularDir(GXLightObj *lt_obj, float nx, float ny, float nz);

static inline u32 fbits(float f) { u32 u; __builtin_memcpy(&u, &f, 4); return u; }

const char *gc_scenario_label(void) { return "GXInitSpecularDir/generic"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_init_specular_dir_generic_001.bin"; }

void gc_scenario_run(GcRam *ram) {
  GXLightObj obj;
  __builtin_memset(&obj, 0, sizeof(obj));
  GXInitSpecularDir(&obj, 0.1f, 0.2f, 0.3f);

  uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x40);
  if (!p) die("gc_ram_ptr failed");
  for (u32 i = 0; i < 0x40; i += 4) wr32be(p + i, 0);
  wr32be(p + 0x00, 0xDEADBEEFu);
  wr32be(p + 0x04, fbits(obj.ldir[0]));
  wr32be(p + 0x08, fbits(obj.ldir[1]));
  wr32be(p + 0x0C, fbits(obj.ldir[2]));
  wr32be(p + 0x10, fbits(obj.lpos[0]));
  wr32be(p + 0x14, fbits(obj.lpos[1]));
  wr32be(p + 0x18, fbits(obj.lpos[2]));
}
