# MP4 HuSysInit: next 10 SDK calls (legacy audit)

Source: `src/game_workload/mp4/vendor/src/game/init.c` (`HuSysInit` + helpers).

These are the next 10 SDK functions in the MP4 init chain, and whether legacy tests can be migrated as-is.

Legend:
- **legacy-oracle**: legacy DOL compiles/runs real decomp SDK code (valid Dolphin oracle).
- **legacy-synthetic**: legacy DOL implements a stub/mock version inside the test (not a valid oracle).
- **missing**: no legacy DOL found.

1) `OSInit`
- Legacy: `docs/_legacy/tests/dols/os_init/generic/os_init_001`
- Status: **legacy-oracle** (Makefile compiles decomp SDK sources under `decomp_mario_party_4/src/dolphin/...`).

2) `DVDInit`
- Legacy: `docs/_legacy/tests/dols/dvd_init/generic/dvd_init_001`
- Status: **legacy-synthetic** (uses libogc `DVD_Init` / `SYS_Init`, not MP4 decomp SDK `DVDInit`).

3) `VIInit`
- Legacy: `docs/_legacy/tests/dols/vi_init/generic/vi_init_001`
- Status: **legacy-synthetic** (defines `VIInit`/`VIGetTvFormat` in the test file).

4) `PADInit`
- Legacy: `docs/_legacy/tests/dols/pad_init/generic/pad_init_001`
- Status: **legacy-synthetic** (defines `PADInit` in the test file and stubs dependencies).

5) `OSGetProgressiveMode`
- Legacy: `docs/_legacy/tests/dols/os_get_progressive_mode/generic/os_get_progressive_mode_001`
- Status: **legacy-synthetic** (mocks SRAM lock/unlock + flags in the test file).

6) `VIGetDTVStatus`
- Legacy: `docs/_legacy/tests/dols/vi_get_dtv_status/generic/vi_get_dtv_status_001`
- Status: **legacy-synthetic** (mocks VI regs + interrupt funcs in the test file).

7) `VIGetTvFormat`
- Legacy: `docs/_legacy/tests/dols/vi_get_tv_format/generic/vi_get_tv_format_001`
- Status: **legacy-synthetic** (mocks CurrTvMode + interrupt funcs in the test file).

8) `OSPanic`
- Legacy: `docs/_legacy/tests/dols/os_panic/generic/os_panic_001`
- Status: **legacy-synthetic** (models OSPanic using stubbed OSReport/PPCHalt etc).

9) `GXAdjustForOverscan`
- Legacy: `docs/_legacy/tests/dols/gx_adjust_for_overscan/generic/gx_adjust_for_overscan_001`
- Status: **legacy-synthetic** (pure math; useful algorithm sketch, but not sourced from decomp SDK).

10) `OSGetArenaHi`
- Legacy: no direct `os_get_arena_hi_001` found.
- Status: **missing** (but we now have canon tests under `tests/sdk/os/os_set_arena_hi/*` that validate `OSSet/GetArenaHi` bit-exact).
