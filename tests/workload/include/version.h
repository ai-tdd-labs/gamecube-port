#pragma once

// Host workload "version.h" shim.
//
// The real decomp has a richer build-system that defines these.
// For host workloads we hardcode MP4 USA/NTSC behavior.

#ifndef VERSION_JP
#define VERSION_JP 0
#endif

#ifndef VERSION_ENG
#define VERSION_ENG 1
#endif

#ifndef REFRESH_RATE
#define REFRESH_RATE 1
#endif

