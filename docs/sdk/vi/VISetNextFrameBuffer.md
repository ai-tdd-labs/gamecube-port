# VISetNextFrameBuffer

## Signature
`void VISetNextFrameBuffer(void *fb)`

## API Contract (from decomp)
- Requires `fb` to be 32-byte aligned (TP/AC decomp logs mention alignment).
- Updates VI global state for the next XFB to display.

## Callsites

### MP4 (Mario Party 4)
- `decomp_mario_party_4/src/game/init.c:171` `VISetNextFrameBuffer(DemoFrameBuffer1);`
- `decomp_mario_party_4/src/game/init.c:220` `VISetNextFrameBuffer(DemoCurrentBuffer);`
- `decomp_mario_party_4/src/dolphin/demo/DEMOInit.c:122` `VISetNextFrameBuffer(DemoFrameBuffer1);`
- `decomp_mario_party_4/src/dolphin/demo/DEMOInit.c:151` `VISetNextFrameBuffer(DemoCurrentBuffer);`

### TP (Twilight Princess)
- `decomp_twilight_princess/src/dolphin/demo/DEMOInit.c:130` `VISetNextFrameBuffer(DemoFrameBuffer1);`
- `decomp_twilight_princess/src/dolphin/demo/DEMOInit.c:181` `VISetNextFrameBuffer(DemoCurrentBuffer);`
- `decomp_twilight_princess/src/dolphin/vi/vi.c` (implementation)
- `decomp_twilight_princess/src/revolution/vi/vi.c` (implementation)
- `decomp_twilight_princess/src/dolphin/os/OSFatal.c:228` `VISetNextFrameBuffer(xfb);`

### WW (Wind Waker)
- `decomp_wind_waker/src/JSystem/JUtility/JUTVideo.cpp` (multiple callsites)
- `decomp_wind_waker/src/JSystem/JUtility/JUTException.cpp` (exception/fatal)

### AC (Animal Crossing)
- `decomp_animal_crossing/src/static/JSystem/JUtility/JUTVideo.cpp` (multiple callsites)
- `decomp_animal_crossing/src/static/JSystem/JUtility/JUTException.cpp:619` `VISetNextFrameBuffer(begin);`

## Evidence
- Decomp implementation (MP4): `decomp_mario_party_4/src/dolphin/vi.c`
- Decomp implementation (AC): `decomp_animal_crossing/src/static/dolphin/vi/vi.c`
- Decomp implementation (TP): `decomp_twilight_princess/src/dolphin/vi/vi.c`

