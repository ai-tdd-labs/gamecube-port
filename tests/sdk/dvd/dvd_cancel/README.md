# DVDCancel (MP4 trace-replay suite)

MP4 pathstyle callsites cancel active DVD reads (or bail out cleanly if already idle).
This suite validates `DVDCancel()` against Dolphin oracle output.

- `tests/sdk/dvd/dvd_cancel/dol/generic/dvd_cancel_generic_001` — DOL scenario
- `tests/sdk/dvd/dvd_cancel/host/dvd_cancel_generic_001_scenario.c` — host parity scenario
- `tools/run_dvd_cancel_trace.sh` — expected + host compare runner

Observed contract under this trace suite:
- null block returns `-1`
- states `0`, `-1`, `10` return `0` unchanged
- busy-like states (`1`) transition to `10` and return `0`

Evidence:
- `tools/run_dvd_cancel_trace.sh`
- mutation check: `tools/mutations/dvd_cancel_null_to_zero.sh`
