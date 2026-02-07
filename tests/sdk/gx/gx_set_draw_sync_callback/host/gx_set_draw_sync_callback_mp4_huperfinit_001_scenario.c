#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef uint16_t u16;

typedef void (*GXDrawSyncCallback)(u16 token);

GXDrawSyncCallback GXSetDrawSyncCallback(GXDrawSyncCallback cb);

extern uintptr_t gc_gx_token_cb_ptr;

static void cb(u16 token) { (void)token; }

const char *gc_scenario_label(void) { return "gx_set_draw_sync_callback/mp4_huperfinit"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_draw_sync_callback_mp4_huperfinit_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    gc_gx_token_cb_ptr = 0;

    GXDrawSyncCallback old0 = GXSetDrawSyncCallback(cb);
    GXDrawSyncCallback old1 = GXSetDrawSyncCallback((GXDrawSyncCallback)0);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x10);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, (old0 == (GXDrawSyncCallback)0) ? 1u : 0u);
    wr32be(p + 0x08, (((GXDrawSyncCallback)gc_gx_token_cb_ptr) == (GXDrawSyncCallback)0) ? 1u : 0u);
    wr32be(p + 0x0C, (old1 == cb) ? 1u : 0u);
}
