# PADControlMotor (MP4 realistic HuPadInit)

MP4 `HuPadInit` calls `PADControlMotor(i, PAD_MOTOR_STOP_HARD)` for each channel
after initial reads.

This suite verifies our deterministic `sdk_port` model for:
- storing the last motor command per channel in RAM-backed state

Oracle:
- Dolphin expected (`dol/mp4/...`) vs host actual (`host/...`) on the RAM dump at `0x80300000`.

