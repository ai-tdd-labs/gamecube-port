# OSDisableInterrupts / OSRestoreInterrupts (MP4 realistic)

This test covers the nested critical-section pattern used by MP4 (HuPadInit and many SDK subsystems):

1. `OSDisableInterrupts()` (outer)
2. `OSDisableInterrupts()` (inner)
3. `OSRestoreInterrupts(inner_level)`
4. `OSRestoreInterrupts(outer_level)`

Oracle:
- Dolphin expected (`dol/mp4/...`) vs host actual (`host/...`) on the RAM dump at `0x80300000`.

## RVZ trace replay (secondary oracle)

We can also replay **retail MP4 RVZ** entry/exit snapshots (harvested via Dolphin GDB breakpoints)
as a host-only check.

- Scenario: `host/os_disable_interrupts_rvz_trace_replay_001_scenario.c`
- Inputs (local-only, gitignored): `tests/traces/os_disable_interrupts/mp4_rvz/*/in_regs.json` (MSR)

Important: retail `OSDisableInterrupts` does not write to a synthetic state page at `0x817FE000`.
So this replay checks only a derived invariant from MSR:
- expected return value is MSR[EE] at entry
- after the call, our model state is "interrupts disabled"

Run one case (auto-derives MSR[EE] from the trace):

```bash
CASE_DIR="tests/traces/os_disable_interrupts/mp4_rvz/<case_id>"
tools/replay_trace_case_os_disable_interrupts.sh "$CASE_DIR"
```

Output marker (`0x80300000`):
- `0xDEADBEEF`: invariant holds (return value + disabled state + call counter)
- `0x0BADF00D`: invariant failed (needs investigation)
