#include <stdint.h>

// Host workload stub for MP4's WipeInit().
//
// Used by init-to-VIWait reachability workloads where we do not want to pull in
// the wipe state machine yet.
void WipeInit(void *render_mode) { (void)render_mode; }

