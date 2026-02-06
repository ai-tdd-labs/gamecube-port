# OSSetCurrentHeap testcases

These testcases validate the behavior of `OSSetCurrentHeap()` against Dolphin and our `src/sdk_port/` implementation.

Case IDs:
- `generic/min_001`: minimal heap setup then set current heap.
- `mp4/realistic_initmem_001`: mirrors MP4 `InitMem` heap bootstrap (after `OSInitAlloc` + `OSCreateHeap`).

