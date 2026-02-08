# MTXOrtho (C_MTXOrtho)

Deterministic math test: build an orthographic 4x4 projection matrix.

Oracle:
- SDK implementation: `decomp_mario_party_4/src/dolphin/mtx/mtx44.c:C_MTXOrtho`

This suite uses parameters `t=0, b=1, l=0, r=1, n=0, f=1` so all relevant
floats are exactly representable (avoids cross-platform float drift).

