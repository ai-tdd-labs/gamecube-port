#pragma once

// MP4 workload: minimal stub header. pad.c includes this but HuPadInit does not
// depend on any MSM interfaces directly.

#include <stdlib.h> // abs()

void msmSysRegularProc(void);
