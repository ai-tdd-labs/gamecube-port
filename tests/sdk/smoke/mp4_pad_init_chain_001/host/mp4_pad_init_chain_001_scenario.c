#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "harness/gc_host_ram.h"

#include "gc_mem.h"
#include "sdk_state.h"

// OS
void OSInit(void);
int OSDisableInterrupts(void);
int OSRestoreInterrupts(int level);

// VI
void VIInit(void);
void VIWaitForRetrace(void);
typedef void (*VIRetraceCallback)(uint32_t retraceCount);
VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback cb);

// SI
void SISetSamplingRate(uint32_t msec);

// PAD
int PADInit(void);
void PADControlMotor(int32_t chan, uint32_t cmd);

static inline void store_u32be(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)(v >> 0);
}

enum { SNAPSHOT_ADDR = 0x80300100u };

// Fixed "pointer token" so PPC-vs-host dumps can match.
#define POST_CB ((VIRetraceCallback)(uintptr_t)0x80001234u)

static void emit_snapshot(GcRam *ram) {
  uint8_t *out = gc_ram_ptr(ram, SNAPSHOT_ADDR, 0x40);
  if (!out) return;

  store_u32be(out + 0x00, 0x534D4F4Bu); // "SMOK"
  store_u32be(out + 0x04, 0x00000001u); // version

  store_u32be(out + 0x08, gc_sdk_state_load_u32be(GC_SDK_OFF_OS_INTS_ENABLED));
  store_u32be(out + 0x0C, gc_sdk_state_load_u32be(GC_SDK_OFF_OS_DISABLE_CALLS));
  store_u32be(out + 0x10, gc_sdk_state_load_u32be(GC_SDK_OFF_OS_RESTORE_CALLS));

  store_u32be(out + 0x14, gc_sdk_state_load_u32be(GC_SDK_OFF_VI_POST_CB_PTR));
  store_u32be(out + 0x18, gc_sdk_state_load_u32be(GC_SDK_OFF_VI_POST_CB_SET_CALLS));
  store_u32be(out + 0x1C, gc_sdk_state_load_u32be(GC_SDK_OFF_VI_WAIT_RETRACE_CALLS));

  store_u32be(out + 0x20, gc_sdk_state_load_u32be(GC_SDK_OFF_SI_SAMPLING_RATE));
  store_u32be(out + 0x24, gc_sdk_state_load_u32be(GC_SDK_OFF_SI_SETXY_LINE));
  store_u32be(out + 0x28, gc_sdk_state_load_u32be(GC_SDK_OFF_SI_SETXY_COUNT));
  store_u32be(out + 0x2C, gc_sdk_state_load_u32be(GC_SDK_OFF_SI_SETXY_CALLS));

  store_u32be(out + 0x30, gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 0u));
  store_u32be(out + 0x34, gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 4u));
  store_u32be(out + 0x38, gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 8u));
  store_u32be(out + 0x3C, gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 12u));
}

enum { PAD_MOTOR_STOP_HARD = 2 };

int main(void) {
  GcRam ram;
  if (gc_ram_init(&ram, 0x80000000u, 0x01800000u) != 0) return 2;
  gc_mem_set(0x80000000u, 0x01800000u, ram.buf);
  gc_sdk_state_reset();

  // Run the same SDK-only chain as the DOL smoke.
  VIInit();
  PADInit();

  gc_sdk_state_store_u16be_mirror(GC_SDK_OFF_VI_REGS_U16BE + (54u * 2u), 0, 0);
  SISetSamplingRate(0);

  int lvl = OSDisableInterrupts();
  (void)VISetPostRetraceCallback(POST_CB);
  OSRestoreInterrupts(lvl);

  VIWaitForRetrace();
  VIWaitForRetrace();

  for (int i = 0; i < 4; i++) PADControlMotor(i, PAD_MOTOR_STOP_HARD);

  uint8_t *marker = gc_ram_ptr(&ram, 0x80300000u, 8);
  if (marker) {
    store_u32be(marker + 0x00, 0xC0DEF00Du);
    store_u32be(marker + 0x04, 0x00000009u);
  }
  emit_snapshot(&ram);

  const char *out_rel = "tests/sdk/smoke/mp4_pad_init_chain_001/actual/mp4_pad_init_chain_001_mem1.bin";
  const char *root = getenv("GC_REPO_ROOT");
  char out_path[4096];
  if (root && root[0]) {
    snprintf(out_path, sizeof(out_path), "%s/%s", root, out_rel);
  } else {
    snprintf(out_path, sizeof(out_path), "%s", out_rel);
  }

  int rc = gc_ram_dump(&ram, 0x80000000u, 0x01800000u, out_path);
  gc_ram_free(&ram);
  return rc;
}
