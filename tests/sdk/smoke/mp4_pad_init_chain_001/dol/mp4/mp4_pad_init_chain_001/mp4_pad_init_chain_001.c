#include <stdint.h>

// This smoke chain runs our sdk_port implementation on real PPC (in Dolphin).
// Oracle for phase A: "PPC build of sdk_port" vs "host build of sdk_port".

#include "src/sdk_port/sdk_state.h"

// Compiled as separate translation units in oracle_*.c to avoid macro/enum clashes.
void VIInit(void);
void VIWaitForRetrace(void);
typedef void (*VIRetraceCallback)(uint32_t retraceCount);
VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback cb);

void SISetSamplingRate(uint32_t msec);

int OSDisableInterrupts(void);
int OSRestoreInterrupts(int level);

int PADInit(void);
void PADSetSpec(uint32_t spec);
void PADControlMotor(int32_t chan, uint32_t cmd);

enum { SNAPSHOT_ADDR = 0x80300100u };

static inline void gc_disable_interrupts(void) {
  uint32_t msr;
  __asm__ volatile("mfmsr %0" : "=r"(msr));
  msr &= ~0x8000u; // MSR[EE]
  __asm__ volatile("mtmsr %0; isync" : : "r"(msr));
}

static inline void gc_arm_decrementer_far_future(void) {
  __asm__ volatile(
      "lis 3,0x7fff\n"
      "ori 3,3,0xffff\n"
      "mtdec 3\n"
      :
      :
      : "r3");
}

static inline void gc_safepoint(void) {
  gc_disable_interrupts();
  gc_arm_decrementer_far_future();
}

static inline void store_u32be_ptr(volatile uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)(v >> 0);
}

// Use a real function pointer so VIWaitForRetrace can safely invoke it.
// Then normalize the stored pointer value in sdk_state to a stable token so
// PPC-vs-host dumps can match bit-exact.
enum { POST_CB_TOKEN = 0x80001234u };
static void PostCB(uint32_t retraceCount) { (void)retraceCount; }

static void emit_snapshot(void) {
  volatile uint8_t *out = (volatile uint8_t *)SNAPSHOT_ADDR;

  store_u32be_ptr(out + 0x00, 0x534D4F4Bu); // "SMOK"
  store_u32be_ptr(out + 0x04, 0x00000001u); // version

  // OS ints
  store_u32be_ptr(out + 0x08, gc_sdk_state_load_u32be(GC_SDK_OFF_OS_INTS_ENABLED));
  store_u32be_ptr(out + 0x0C, gc_sdk_state_load_u32be(GC_SDK_OFF_OS_DISABLE_CALLS));
  store_u32be_ptr(out + 0x10, gc_sdk_state_load_u32be(GC_SDK_OFF_OS_RESTORE_CALLS));

  // VI post-cb
  store_u32be_ptr(out + 0x14, gc_sdk_state_load_u32be(GC_SDK_OFF_VI_POST_CB_PTR));
  store_u32be_ptr(out + 0x18, gc_sdk_state_load_u32be(GC_SDK_OFF_VI_POST_CB_SET_CALLS));
  store_u32be_ptr(out + 0x1C, gc_sdk_state_load_u32be(GC_SDK_OFF_VI_WAIT_RETRACE_CALLS));

  // SI sampling
  store_u32be_ptr(out + 0x20, gc_sdk_state_load_u32be(GC_SDK_OFF_SI_SAMPLING_RATE));
  store_u32be_ptr(out + 0x24, gc_sdk_state_load_u32be(GC_SDK_OFF_SI_SETXY_LINE));
  store_u32be_ptr(out + 0x28, gc_sdk_state_load_u32be(GC_SDK_OFF_SI_SETXY_COUNT));
  store_u32be_ptr(out + 0x2C, gc_sdk_state_load_u32be(GC_SDK_OFF_SI_SETXY_CALLS));

  // PAD motor cmds
  store_u32be_ptr(out + 0x30, gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 0u));
  store_u32be_ptr(out + 0x34, gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 4u));
  store_u32be_ptr(out + 0x38, gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 8u));
  store_u32be_ptr(out + 0x3C, gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 12u));
}

enum { PAD_MOTOR_STOP_HARD = 2 };
enum { PAD_SPEC_5 = 5 };

int main(void) {
  gc_safepoint();

  gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);
  gc_sdk_state_reset();

  VIInit();
  gc_safepoint();
  PADSetSpec(PAD_SPEC_5);
  gc_safepoint();
  PADInit();
  gc_safepoint();

  // HuPadInit: SISetSamplingRate(0)
  gc_sdk_state_store_u16be_mirror(GC_SDK_OFF_VI_REGS_U16BE + (54u * 2u), 0, 0);
  SISetSamplingRate(0);
  gc_safepoint();

  // HuPadInit: disable -> set post cb -> restore
  int lvl = OSDisableInterrupts();
  (void)VISetPostRetraceCallback(PostCB);
  gc_sdk_state_store_u32be(GC_SDK_OFF_VI_POST_CB_PTR, (uint32_t)POST_CB_TOKEN);
  OSRestoreInterrupts(lvl);
  gc_safepoint();

  // HuPadInit: wait retrace x2
  VIWaitForRetrace();
  gc_safepoint();
  VIWaitForRetrace();
  gc_safepoint();

  // HuPadInit: stop motors
  for (int i = 0; i < 4; i++) {
    PADControlMotor(i, PAD_MOTOR_STOP_HARD);
    gc_safepoint();
  }

  *(volatile uint32_t *)0x80300000 = 0xC0DEF00Du;
  *(volatile uint32_t *)0x80300004 = 0x00000009u; // number of steps above (rough)
  emit_snapshot();

  while (1) {
    gc_safepoint();
    __asm__ volatile("nop");
  }
  return 0;
}
