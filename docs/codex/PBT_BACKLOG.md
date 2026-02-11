# PBT Backlog (Nintendo SDK)

This file tracks PBT-capable SDK functions/suites for this repo.

## Completed

- `os_round_32b` (`OSRoundUp32B`, `OSRoundDown32B`) - PASS (200k)
- `os_arena` (`OSSetArenaLo/Hi`, `OSGetArenaLo/Hi`) - PASS (200k)
- `os_interrupts` (`OSDisableInterrupts`, `OSEnableInterrupts`, `OSRestoreInterrupts`) - PASS (200k)
- `os_module_queue` (`OSLink`, `OSUnlink`) - PASS (200k)
- `si_sampling_rate` (`SISetSamplingRate`) - PASS (200k)
- `si_get_response` (`SIGetResponse`) - PASS (200k)
- `si_transfer` (`SITransfer`) - PASS (200k)
- `pad_control_motor` (`PADControlMotor`) - PASS (200k)
- `pad_reset_clamp` (`PADReset`, `PADClamp`) - PASS (200k)
- `vi_post_retrace_callback` (`VISetPostRetraceCallback` + retrace dispatch) - PASS (200k)
- `dvd_read_prio` (`DVDReadPrio`) - PASS (200k)
- `dvd_read_async_prio` (`DVDReadAsyncPrio`) - PASS (200k)
- `dvd_convert_path` (`DVDConvertPathToEntrynum`) - PASS (200k)
- `gx_texcopy_relation` (`GXSetTexCopyDst`, `GXGetTexBufferSize`) - PASS (200k)
- `gx_vtxdesc_packing` (`GXSetVtxDesc`, `GXSetVtxAttrFmt`) - PASS (200k)
- `gx_alpha_tev_packing` (`GXSetTev*`, `GXSetAlphaCompare`, `GXSetAlphaUpdate`) - PASS (200k)
- `mtx_core` (MTX identity/ortho/vector core) - PASS
- `dvd_core` (`DVDRead*`, `DVDConvertPathToEntrynum`, `DVDFastOpen`, `DVDClose`) - PASS
- `arq_property` - PASS
- `card_fat_property` - PASS

## Open BD Tasks (next candidates)

- `gamecube-mvl` - OSUnlink trigger via overlay transition (retail harvest blocker)

## Candidate pool after current BD list

These are PBT-friendly once the current queue is done:

- OS cache helpers (`DCStoreRangeNoSync`) for deterministic range/alignment properties
- OS fast-cast/init helpers (`OSInitFastCast`) state-idempotency properties
- VI simple state APIs (`VISetBlack`, `VIFlush`) call-count/state properties
- PAD init/spec APIs (`PADInit`, `PADSetSpec`) deterministic state transitions
