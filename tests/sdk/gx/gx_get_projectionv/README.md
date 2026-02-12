# GXGetProjectionv

- Subsystem: GX
- Callsite basis: MP4 shadow/map paths that save current projection (`GXGetProjectionv`).
- Scenario: set projection with an orthographic matrix, then read it back with `GXGetProjectionv`.

Expected behavior:
- `out[0]` stores projection type as float (`GX_ORTHOGRAPHIC` => `1.0f`).
- `out[1..6]` return packed projection values from the most recent `GXSetProjection`.
- `GXGetProjectionv` does not mutate GX BP/XF state.
