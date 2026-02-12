# VISetPreRetraceCallback

Generic deterministic test for `VISetPreRetraceCallback` — symmetric to VISetPostRetraceCallback.

Tests:
- First set returns NULL (no previous callback)
- Second set returns previous callback pointer (CB_A)
- sdk_state records correct callback pointer and call count

Oracle:
- expected: DOL in Dolphin (sdk_port on PPC)
- actual: host scenario (sdk_port on host)
