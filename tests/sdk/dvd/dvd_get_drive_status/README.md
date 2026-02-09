# DVDGetDriveStatus (MP4 HuDvdErrorWatch)

MP4 calls `DVDGetDriveStatus()` from its error-watch logic (see `tests/workload/mp4/slices/hudvderrorwatch_only.c`).

Decomp reference:
- `decomp_mario_party_4/src/dolphin/dvd/dvd.c` `DVDGetDriveStatus()`

Observed contract (high-level):
- `-1` if a fatal error flag is set
- `8` if the system is pausing
- `0` when no command block is executing (idle)
- otherwise delegates to `DVDGetCommandBlockStatus(executing)`

This suite currently validates the **idle path** used during normal boot:
- `DVDInit(); DVDGetDriveStatus();` must return `0`.

Oracle:
- Dolphin expected (`dol/mp4/...`) vs host actual (`host/...`) on the RAM dump at `0x80300000`.
