#pragma once

#include "gc_host_ram.h"

// Scenario contract for the generic host runner.
//
// Each scenario:
// - mutates SDK port state via src/sdk_port/*
// - writes marker/results into virtual RAM (typically at 0x80300000)
// - returns a relative output path for the RAM dump
//
// The runner then dumps a fixed RAM region to that path.

const char *gc_scenario_label(void);
const char *gc_scenario_out_path(void);
void gc_scenario_run(GcRam *ram);

