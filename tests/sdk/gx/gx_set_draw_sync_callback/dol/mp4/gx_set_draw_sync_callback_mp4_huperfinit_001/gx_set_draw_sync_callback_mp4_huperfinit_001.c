#include <stdint.h>

typedef uint32_t u32;
typedef uint16_t u16;
typedef int BOOL;

typedef void (*GXDrawSyncCallback)(u16 token);

static GXDrawSyncCallback s_token_cb;

static BOOL OSDisableInterrupts(void) { return 1; }
static int OSRestoreInterrupts(int enabled) { return enabled; }

static GXDrawSyncCallback GXSetDrawSyncCallback(GXDrawSyncCallback cb) {
    GXDrawSyncCallback oldcb;
    BOOL enabled;

    oldcb = s_token_cb;
    enabled = OSDisableInterrupts();
    s_token_cb = cb;
    OSRestoreInterrupts(enabled);
    return oldcb;
}

static void cb(u16 token) { (void)token; }

int main(void) {
    s_token_cb = (GXDrawSyncCallback)0;

    GXDrawSyncCallback old0 = GXSetDrawSyncCallback(cb);
    GXDrawSyncCallback old1 = GXSetDrawSyncCallback((GXDrawSyncCallback)0);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = (old0 == (GXDrawSyncCallback)0) ? 1u : 0u;
    *(volatile u32 *)0x80300008 = (s_token_cb == (GXDrawSyncCallback)0) ? 1u : 0u;
    *(volatile u32 *)0x8030000C = (old1 == cb) ? 1u : 0u;
    while (1) {}
}
