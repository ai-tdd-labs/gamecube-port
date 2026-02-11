# OSThread + OSMutex Oracle Delta

## Oracle classification: DECOMP_ADAPTED

The oracle is an exact copy of decomp logic with minor adaptations for the
single-threaded test environment.

## Decomp sources

- `external/mp4-decomp/src/dolphin/os/OSThread.c`
- `external/mp4-decomp/src/dolphin/os/OSMutex.c`
- `external/mp4-decomp/include/dolphin/os/OSThread.h`
- `external/mp4-decomp/include/dolphin/os/OSMutex.h`

## Adaptations from decomp

### 1. No OSContext / stack pointers

The decomp OSThread struct has `OSContext context`, `u32* stackBase`,
`u32* stackEnd`. These are stripped — no real PPC context to save/load.

Thread struct is a compact 14-field layout (id, state, attr, suspend,
priority, base, val, queue, link, queueJoin, mutex, queueMutex, linkActive).

### 2. No interrupt disable/restore

All `OSDisableInterrupts()` / `OSRestoreInterrupts()` calls in the decomp are
removed. The oracle runs single-threaded; no concurrency to protect against.

### 3. Blocking API single-iteration (LockMutex, JoinThread)

**Decomp:** `OSLockMutex` has a `while(TRUE)` loop that spans context switches.
After `OSSleepThread`, the thread is rescheduled and re-enters the loop.

**Oracle:** Single iteration only. If contended, calls `OSSleepThread` and returns.
The test driver calls `oracle_ProcessPendingLocks()` after each operation to
complete deferred re-acquisitions (models the while loop re-entry).

**Rationale:** `OSSleepThread` changes `oracle_CurrentThread` but the C call stack
continues with a stale local pointer. Running the loop would corrupt queues.

### 4. __OSPromoteThread (reconstructed)

Not present in the mp4-decomp. Reconstructed from SDK behavior:
```c
static void oracle_OSPromoteThread(oracle_OSThread *thread, int32_t priority) {
    do {
        if (0 < thread->suspend) break;
        if (thread->priority <= priority) break;
        thread = oracle_SetEffectivePriority(thread, priority);
    } while (thread);
}
```

Unlike `UpdatePriority` (which recalculates effective priority from owned mutexes),
`PromoteThread` directly sets a higher priority and chains through mutex owners.

### 5. OSJoinThread (reconstructed, non-blocking only)

Not implemented in mp4-decomp (only declared in header). Oracle implements the
non-blocking path: if target is MORIBUND, collect val, remove from active queue,
clear state. The blocking path (sleep on queueJoin) is not exercised in tests.

### 6. OSWaitCond (not tested in random mix)

`OSWaitCond` releases a mutex, sleeps on a cond queue, then re-locks the mutex
with saved count. The re-lock after sleep has the same blocking-loop issue as
`OSLockMutex`. Currently implemented but excluded from random test mix.

## Fields compared (oracle vs port)

### Per-thread
- state, attr, suspend, priority, base, val
- mutex (GC addr of mutex being waited on)
- queueMutex.head, queueMutex.tail (owned mutex list)

### Per-mutex
- queue.head, queue.tail (waiting thread queue)
- thread (owner)
- count (recursive lock depth)
- link.next, link.prev (in owner's queueMutex)

### Global
- currentThread, runQueueBits, reschedule

## Test levels

| Level | Description | Operations |
|-------|-------------|------------|
| L0 | Basic lifecycle | Create, Resume |
| L1 | Suspend/Resume | Random suspend/resume cycles |
| L2 | Sleep/Wakeup | Sleep, Wakeup, Yield on wait queues |
| L3 | Random integration | All thread + mutex ops mixed |
| L4 | Mutex basic | TryLock, Lock, Unlock random mix |
| L5 | Priority inheritance | Contended lock → promote → unlock → restore |
| L6 | JoinThread | Exit → MORIBUND → Join collects val |

## Verification

```bash
tools/run_osthread_property_test.sh --num-runs=2000
# Result: 469172/469172 PASS (2000 seeds, 7 levels)
```
