# OSStopwatch (MP4 HuPerf)

MP4 callsites:
- `HuPerfZero`: `OSStopStopwatch(&Ssw); OSResetStopwatch(&Ssw); OSStartStopwatch(&Ssw);`
- `HuPerfBegin/End`: `OSStartStopwatch`, `OSCheckStopwatch`, `OSStopStopwatch`, `OSResetStopwatch`
  Evidence: `<DECOMP_MP4_ROOT>/src/game/perf.c`

SDK reference behavior:
- `OSStartStopwatch`: sets `running=1`, `last=OSGetTime()`
- `OSStopStopwatch`: if running, accumulates interval into total, bumps hits, updates min/max
- `OSCheckStopwatch`: returns total plus (now-last) if running
- `OSResetStopwatch`: calls `OSInitStopwatch(sw, sw->name)`
  Evidence: `<DECOMP_MP4_ROOT>/src/dolphin/os/OSStopwatch.c`

Oracle:
- expected: PPC DOL with reference behavior (run in Dolphin)
- actual: host scenario using `<REPO_ROOT>/src/sdk_port/os/OSStopwatch.c`

Determinism:
- For these tests, `OSGetTime()` is modeled as a monotonic counter incremented per call.
