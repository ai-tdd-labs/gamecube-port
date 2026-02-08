// Minimal decomp slice: HuSoftResetButtonCheck() from MP4 sreset.c.
// Keep behavior exact for the early-boot/mainloop check: if SR_ExecReset is set,
// call HuRestartSystem() and return 1; otherwise return 0.
//
// This function is game-specific (not Nintendo SDK). We keep it as a slice so
// we can progressively replace host-only stubs without importing the full reset
// subsystem.

#include <stdint.h>

typedef int32_t s32;

// In the full game this global lives in sreset.c.
// It is checked each frame in main.c.
s32 SR_ExecReset = 0;

// Stub: full implementation requires many subsystems; not needed unless SR_ExecReset is set.
void HuRestartSystem(void) {}

s32 HuSoftResetButtonCheck(void) {
    if (SR_ExecReset) {
        HuRestartSystem();
    }
    return (SR_ExecReset) ? 1 : 0;
}
