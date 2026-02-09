#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

extern uint32_t gc_gx_texcoord2f32_s_bits;
extern uint32_t gc_gx_texcoord2f32_t_bits;

typedef uint32_t u32;
typedef uint8_t u8;

typedef struct { uint8_t _dummy; } GXFifoObj;
GXFifoObj *GXInit(void *base, u32 size);
void GXTexCoord2f32(float s, float t);

const char *gc_scenario_label(void) { return "GXTexCoord2f32/mp4_pfdrawfonts"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_tex_coord_2f32_mp4_pfdrawfonts_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    GXTexCoord2f32(0.25f, 0.5f);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x0C);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_texcoord2f32_s_bits);
    wr32be(p + 0x08, gc_gx_texcoord2f32_t_bits);
}

