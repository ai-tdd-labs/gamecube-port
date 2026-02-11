# ORACLE DELTA — ARQ

Oracle file: `tests/sdk/ar/property/arq_oracle.h`
Decomp source: `/Users/chrislamark/projects/recomp/gamecube_static_recomp/decomp_mario_party_4/src/dolphin/ar/arq.c`
Tier: `DECOMP_ADAPTED`

## Exactly matched behavior
- Queue insert order for low/high priority requests.
- Hi queue pop DMA argument selection for MRAM<->ARAM direction.
- Lo queue chunking algorithm (`length/source/dest` decrement/increment).
- Interrupt service ordering: hi callback path first, else low, then queue service.

## Intentional deltas from decomp
- Callback function pointers are replaced by boolean/counter tracking.
- `OSDisableInterrupts/OSRestoreInterrupts` is omitted in oracle.
- `ARStartDMA` is replaced by DMA log capture (no real hardware side effect).
- `ARQInit` full registration path is reduced to deterministic state reset.

## Coverage impact
- Strong for deterministic queue/chunk math and ordering.
- Not authoritative for interrupt masking semantics or callback ABI behavior.

## Evidence commands
- `tools/run_arq_property_test.sh --num-runs=50 -v`
