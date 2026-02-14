#include <stdint.h>

typedef uint32_t u32;
typedef int32_t s32;

// Oracle behavior matches the sdk_port EXI "host-test knobs" contract:
// - ProbeEx returns: -1 no device, 0 busy, >0 present
// - GetID returns 1 on success, 0 on failure.
s32 gc_exi_probeex_ret[3] = { -1, -1, -1 };
u32 gc_exi_getid_ok[3];
u32 gc_exi_id[3];

s32 oracle_EXIProbeEx(s32 channel) {
  if (channel < 0 || channel >= 3) return -1;
  return gc_exi_probeex_ret[channel];
}

u32 oracle_EXIProbe(s32 channel) {
  if (channel < 0 || channel >= 3) return 0;
  return oracle_EXIProbeEx(channel) > 0;
}

s32 oracle_EXIGetID(s32 channel, u32 device, u32* id) {
  (void)device;
  if (channel < 0 || channel >= 3) return 0;
  if (!gc_exi_getid_ok[channel]) {
    if (id) *id = 0;
    return 0;
  }
  if (id) *id = gc_exi_id[channel];
  return 1;
}

