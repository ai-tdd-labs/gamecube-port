#include <stdint.h>

// Host-only stubs for heavy MP4 game modules after HuSprInit.
//
// These are NOT Nintendo SDK ports and NOT correctness oracles.
// Purpose: allow the host workload to reach later SDK calls (e.g. VIWaitForRetrace)
// without pulling the entire game engine/decomp into the repo.

void HuDataInit(void) {}
void WipeInit(void *render_mode) { (void)render_mode; }

void omMasterInit(int a0, void *ovltbl, int ovl_count, int ovl_boot)
{
    (void)a0;
    (void)ovltbl;
    (void)ovl_count;
    (void)ovl_boot;
}
