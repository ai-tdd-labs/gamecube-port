# Nintendo GC SDK — Overzicht Property-Testbare Call Chains

## Context

We hebben OSAlloc succesvol property-getest met het oracle-patroon:
decomp code (exact kopie met `oracle_` prefix) vs sdk_port (big-endian emulatie via `gc_mem`),
-m32 compilatie, zelfde PRNG seed, resultaten vergelijken na elke operatie.

Vraag: welke ANDERE call chains in de Nintendo GC SDK kunnen we op dezelfde manier testen?

Criteria voor een goede kandidaat:
1. Decomp source beschikbaar in `external/mp4-decomp/src/dolphin/`
2. **Pure logica** — geen echte hardware nodig (of hardware te mocken)
3. Niet-triviale algoritmiek (linked lists, state machines, buffers — niet simpele register writes)
4. Al geport in `src/sdk_port/` OF port is haalbaar

---

## De 8 call chains

### Chain 1: OSAlloc — Heap Allocator (KLAAR, 2000/2000 pass)

```
OSAllocFromHeap ─┬─> DLExtract (free list)
                 └─> DLAddFront (allocated list)
OSFreeToHeap    ─┬─> DLExtract (allocated list)
                 └─> DLInsert  (free list, coalescing)
OSCreateHeap    ──> DLInsert (initial free cell)
OSDestroyHeap   ──> (mark hd->size = -1)
OSAddToHeap     ──> DLInsert (extra cell in free list)
OSCheckHeap     ──> walk free + allocated lists
         leaves:  DLAddFront, DLExtract, DLInsert, DLLookup
```

Geport: ja. Getest: ja, alle levels.

---

### Chain 2: MTX — Matrix Math (puur, stateless)

```
C_MTXConcatArray ──> C_MTXConcat ──> C_MTXCopy (leaf)
C_MTXRotRad      ──> sinf/cosf ──> C_MTXRotTrig (leaf)
C_MTXRotAxisRad  ──> sinf/cosf + C_VECNormalize (vec.c)
C_MTXLookAt      ──> VECNormalize + VECCrossProduct (vec.c)
C_MTXInverse     ──> C_MTXCopy (leaf, complex determinant math)
C_MTXReflect     ──> C_VECDotProduct (vec.c)
C_MTXQuat        ──> (leaf, quaternion → matrix)
C_MTXPerspective ──> tanf (leaf)
C_QUATSlerp      ──> acosf + sinf (leaf)
C_VECHalfAngle   ──> VECNormalize + VECAdd + VECDotProduct
         leaves:  C_MTXCopy, C_MTXIdentity, C_MTXScale, C_MTXTrans,
                  C_VECAdd, C_VECSubtract, C_VECDotProduct,
                  C_VECCrossProduct, C_VECNormalize, C_QUATAdd
```

**Decomp**: `dolphin/mtx/mtx.c`, `mtx44.c`, `vec.c`, `quat.c`, `mtxvec.c`
**Geport**: deels (`mtx.c`, `mtx44.c`). `vec.c`, `quat.c` nog niet.
**Bijzonder**: geen state, puur wiskundig. Oracle = decomp's `C_*` functies,
port = sdk_port versie. Random matrices/vectoren genereren, resultaten vergelijken
(met epsilon voor floating point). ~40 functies totaal.

---

### Chain 3: OSThread — Scheduler + Priority Inheritance (COMPLEX)

```
OSResumeThread  ──> __OSGetEffectivePriority ──> SetRun ──> __OSReschedule ──> SelectThread
OSSuspendThread ──> UnsetRun ──> __OSReschedule ──> SelectThread
OSSleepThread   ──> UnsetRun ──> __OSReschedule ──> SelectThread
OSWakeupThread  ──> SetRun ──> __OSReschedule ──> SelectThread
OSCreateThread  ──> (init context + SetRun)
OSCancelThread  ──> UnsetRun + queue cleanup
OSYieldThread   ──> __OSReschedule ──> SelectThread
         intern:  SetRun (bitmap + queue), UnsetRun (bitmap + queue),
                  SelectThread (highest-priority scan), AddTail, AddPrio,
                  RemoveItem, RemoveHead (queue macros)
         leaves:  queue macros, bitmap operations
```

**Decomp**: `dolphin/os/OSThread.c` (~700 regels, 20+ functies)
**Geport**: NEE — zou nieuw porting werk zijn
**Bijzonder**: vergelijkbaar complex als OSAlloc. Run queue met priority bitmap,
thread state machine, priority inheritance via mutexes. Veel invarianten te testen:
- Bitmap matcht queue membership
- Hoogste prioriteit thread wordt geselecteerd
- Suspend count correct bijgehouden
- Geen thread op meerdere queues tegelijk

---

### Chain 4: OSMutex + Condition Variables (hangt af van OSThread)

```
OSLockMutex     ──> (check owner) ──> OSSleepThread (als bezet)
                ──> UpdatePriority ──> SetEffectivePriority (recursief!)
OSUnlockMutex   ──> __OSGetEffectivePriority ──> OSWakeupThread
OSTryLockMutex  ──> (check owner, non-blocking)
OSWaitCond      ──> OSUnlockMutex ──> OSSleepThread ──> OSLockMutex (atomisch)
         intern:  __OSPromoteThread, __OSCheckDeadLock (recursief)
         leaves:  queue manipulatie, priority berekening
```

**Decomp**: `dolphin/os/OSMutex.c`
**Geport**: NEE
**Bijzonder**: recursieve priority inheritance, deadlock detectie.
VEREIST Chain 3 (OSThread) eerst geport.

---

### Chain 5: OSMessage — Circular Buffer Queue (hangt af van OSThread)

```
OSSendMessage    ──> (queue vol?) ──> OSSleepThread / OSWakeupThread
                 ──> circular buffer: msg[firstIndex + usedCount] = msg
OSReceiveMessage ──> (queue leeg?) ──> OSSleepThread / OSWakeupThread
                 ──> circular buffer: msg = msg[firstIndex++]
OSJamMessage     ──> (queue vol?) ──> firstIndex-- (LIFO push aan kop)
         leaves:  circular buffer index arithmetic, modulo wrapping
```

**Decomp**: `dolphin/os/OSMessage.c` (~150 regels)
**Geport**: NEE
**Bijzonder**: FIFO queue met blocking en priority. OSJamMessage pusht LIFO aan kop.
VEREIST Chain 3 (OSThread) voor blocking semantics, maar buffer logica apart testbaar.

---

### Chain 6: CARD FAT — Memory Card Block Allocator

```
__CARDAllocBlock ──> (circulaire scan door FAT array)
                 ──> fat[prevBlock] = iBlock (chain bouwen)
                 ──> __CARDUpdateFatBlock ──> __CARDCheckSum (leaf)
__CARDFreeBlock  ──> (walk fat chain tot 0xFFFF)
                 ──> fat[block] = 0x0000 (vrij markeren)
                 ──> __CARDUpdateFatBlock ──> __CARDCheckSum (leaf)
         leaves:  __CARDCheckSum, CARDIsValidBlockNo (macro)
```

**Decomp**: `dolphin/card/CARDBlock.c`
**Geport**: NEE
**Bijzonder**: FAT is een uint16 array (block → next-block linked list).
Vergelijkbaar met OSAlloc: alloc scant circular, free walkt chain.
Invarianten: free count klopt, geen cycles, chain eindigt op 0xFFFF.
**Geen hardware dep** in de FAT logica zelf (EXI alleen voor write-back).

---

### Chain 7: ARQ — ARAM DMA Queue Manager

```
ARQPostRequest   ──> queue append (high of low priority)
                 ──> __ARQPopTaskQueueHi (als idle)
                 ──> __ARQServiceQueueLo (als geen high-prio)
__ARQServiceQueueLo ──> chunk splitting (length / chunkSize)
                     ──> source/dest/length update per chunk
__ARQPopTaskQueueHi ──> queue dequeue + state update
         leaves:  linked list append/remove, chunk arithmetic
```

**Decomp**: `dolphin/ar/arq.c`
**Geport**: NEE
**Bijzonder**: twee priority queues + chunking state machine.
Pure logica als je DMA mockt. Testbaar: priority ordering, FIFO binnen level,
chunking arithmetic correct.

---

### Chain 8: DVD FS — File System Path Parser

```
DVDConvertPathToEntrynum ──> isSame (case-insensitive compare, leaf)
                         ──> FST traversal loop (directory entries)
entryToPath              ──> myStrncpy (leaf)
                         ──> recursief parent traversal
DVDOpen ──> DVDConvertPathToEntrynum ──> DVDFastOpen (leaf)
         leaves:  isSame, myStrncpy
```

**Decomp**: `dolphin/dvd/dvdfs.c`
**Geport**: stub (simpele lineaire search)
**Bijzonder**: string parsing + FST tree traversal. Geen hardware dep.
Testbaar: random FST genereren, paths resolven, round-trip testen.

---

## Gedetailleerd voorbeeld: Chain 6 (CARD FAT Allocator)

Dit is de meest directe analogie met OSAlloc. Beide zijn block allocators met linked lists.

### Hoe het werkt

De FAT (File Allocation Table) is een `uint16_t[N]` array waar:
- `fat[0]` = checksum 1
- `fat[1]` = checksum 2
- `fat[2]` = update counter
- `fat[3]` = free block count
- `fat[4]` = last-alloc hint (waar volgende scan start)
- `fat[5..N-1]` = block entries: `0x0000` = vrij, `0xFFFF` = end-of-chain, anders = next block

**AllocBlock(cBlock)**: alloceer `cBlock` aaneengeschakelde blocks
```
start scan bij fat[4] (hint)
loop circular door fat[5..N-1]:
  als fat[i] == 0x0000 (vrij):
    fat[prevBlock] = i    // chain naar vorige
    cBlock--
    als cBlock == 0: break
fat[lastBlock] = 0xFFFF   // end marker
fat[3] -= allocated        // free count update
fat[4] = lastBlock         // hint update
```

**FreeBlock(startBlock)**: free een hele chain
```
walk: block = startBlock
while block != 0xFFFF:
  next = fat[block]
  fat[block] = 0x0000     // mark vrij
  fat[3]++                 // free count++
  block = next
```

### Oracle test opzet (zoals OSAlloc)

```
oracle: exact decomp kopie, native uint16_t fat[] array
port:   sdk_port versie, fat[] in gc_mem (big-endian emulatie)

per seed:
  1. genereer random FAT grootte (64-2048 blocks)
  2. random mix van AllocBlock + FreeBlock operaties
  3. na elke operatie:
     - vergelijk fat[3] (free count)
     - vergelijk returned block chains
     - vergelijk volledige FAT array (byte-voor-byte)
```

### Bottom-up test levels

```
Level 0 (leaves):  __CARDCheckSum — checksum van FAT array
Level 1 (API):     __CARDAllocBlock alleen, __CARDFreeBlock alleen
Level 2 (integratie): random mix alloc + free, FAT vergelijken
```

---

## Samenvatting: hoeveel chains?

| # | Chain | Complexiteit | Geport? | Afh. van |
|---|-------|-------------|---------|----------|
| 1 | **OSAlloc** (heap) | Hoog | Ja | — |
| 2 | **MTX** (matrix math) | Laag-Medium | Deels | — |
| 3 | **OSThread** (scheduler) | Zeer hoog | Nee | — |
| 4 | **OSMutex** (locking) | Hoog | Nee | Chain 3 |
| 5 | **OSMessage** (queue) | Medium | Nee | Chain 3 |
| 6 | **CARD FAT** (block alloc) | Hoog | Nee | — |
| 7 | **ARQ** (DMA queue) | Medium | Nee | — |
| 8 | **DVD FS** (path parser) | Medium | Nee | — |

**Totaal: ~8 call chains** waarvan 1 klaar (OSAlloc).

### Niet meegeteld (te simpel of pure hardware stubs):
- OSStopwatch (5 functies, triviale logica)
- OSCache (hardware stubs, geen logica)
- OSInterrupts (simpele counter)
- OSRtc (1 bit extractie)
- OSError (no-op stubs)
- GX (100+ functies maar alleen register mirrors, geen algoritmes)
- AR alloc (LIFO stack, 2 functies, triviaal)
- PAD/SI/VI (hardware I/O, geen logica)

### Aanbevolen volgorde na OSAlloc:
1. **MTX** — puur, stateless, al deels geport, makkelijkste volgende stap
2. **CARD FAT** — meest vergelijkbaar met OSAlloc, standalone (geen deps)
3. **DVD FS** — string parsing, standalone
4. **ARQ** — priority queues + chunking, standalone
5. **OSThread** → **OSMutex** → **OSMessage** — groot blok, moet samen
