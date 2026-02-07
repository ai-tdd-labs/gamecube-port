#include <stdint.h>

typedef uint32_t u32;
typedef uint16_t u16;
typedef int BOOL;

static u32 s_last_ras_reg;
static u32 s_bp_sent_not;
static u16 s_last_token;

static BOOL OSDisableInterrupts(void) { return 1; }
static int OSRestoreInterrupts(int enabled) { return enabled; }
static void GXFlush(void) {}
static void GX_WRITE_RAS_REG(u32 v) { s_last_ras_reg = v; }

static void GXSetDrawSync(u16 token) {
    BOOL enabled;
    u32 reg;

    enabled = OSDisableInterrupts();
    reg = (u32)token | 0x48000000u;
    GX_WRITE_RAS_REG(reg);
    GX_WRITE_RAS_REG(reg);
    GXFlush();
    OSRestoreInterrupts(enabled);
    s_bp_sent_not = 0;
    s_last_token = token;
}

int main(void) {
    s_last_ras_reg = 0;
    s_bp_sent_not = 1;
    s_last_token = 0;

    GXSetDrawSync((u16)0xFF00u);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_last_ras_reg;
    *(volatile u32 *)0x80300008 = s_bp_sent_not;
    *(volatile u32 *)0x8030000C = (u32)s_last_token;
    while (1) {}
}
