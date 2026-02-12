# GX FIFO PBT — Testing Port Against Real SDK via FIFO Capture

## Het Probleem

Onze sdk_port heeft functies als `GXSetTevOrder` die gebaseerd zijn op de MP4 decomp.
We willen bewijzen dat onze port **exact dezelfde output** produceert als de echte SDK.

Maar de echte SDK (libogc) slaat alles op in een privé struct (`__GXData`).
We kunnen daar niet bij van buitenaf — het is een "keuken met dichte muren".

## De Oplossing: FIFO Capture

Elke GX functie schrijft uiteindelijk bytes naar de **GPU FIFO** (Write Gather Pipe op `0xCC008000`).
Die bytes zijn de echte communicatie tussen CPU en GPU.
Als onze port dezelfde FIFO bytes produceert → onze port is 100% correct.

### Hoe de GX FIFO werkt

```
CPU  →  [0x61] [reg_value 4 bytes]  →  GPU
         ^^^^   ^^^^^^^^^^^^^^^^^^
         opcode  BP register data (big-endian)
```

Voor `GXSetTevOrder` schrijft de SDK precies **5 bytes** per aanroep:
- `0x61` = "BP register write" opcode
- 4 bytes = de register waarde (inclusief register-ID in bovenste byte)

Voorbeeld: `GXSetTevOrder(stage=0, coord=0, map=0, color=GX_ALPHA0)`
→ FIFO bytes: `61 28 00 00 40`
  - `0x28` = register-ID voor tref[0]
  - `0x00 0x00 0x40` = bitfield data (coord=0, map=0, enable=1, color=c2r[4]=0)

### FIFO Redirect

libogc heeft een functie: `GXRedirectWriteGatherPipe(void *buf)`
Deze stuurt alle FIFO writes naar een buffer in geheugen in plaats van naar de GPU.
Na de call kun je de buffer uitlezen om te zien wat er geschreven is.

## Implementatie

### DOL kant (echte SDK)

```c
// 1. Init GX normaal
static u8 fifo[0x40000] __attribute__((aligned(32)));
GXInit(fifo, sizeof(fifo));

// 2. Redirect FIFO naar capture buffer
static u8 capture[4096] __attribute__((aligned(32)));
GXRedirectWriteGatherPipe(capture);

// 3. Reset capture positie
u32 cap_pos = 0;

// 4. Voor elke test case:
GXSetTevOrder(stage, coord, map, color);

// 5. Lees 5 bytes uit capture buffer
//    capture[cap_pos+0] = 0x61 (opcode)
//    capture[cap_pos+1..4] = register waarde (big-endian)
cap_pos += 5;

// 6. Restore FIFO
GXRestoreWriteGatherPipe();

// 7. Dump capture buffer naar output regio (0x80300000)
```

### Host kant (onze port)

```c
// 1. Voor elke test case:
GXSetTevOrder(stage, coord, map, color);

// 2. Construeer de verwachte FIFO bytes:
u32 reg_id = 0x28 + (stage / 2);  // register-ID (tref0=0x28 .. tref7=0x2F)
u32 fifo_val = (reg_id << 24) | (gc_gx_tref[stage / 2] & 0x00FFFFFFu);

// 3. Schrijf als big-endian bytes: 0x61 + fifo_val
```

### Vergelijking

```
DOL capture bytes  ==  Host constructed bytes  →  PASS
```

## Register-ID Verschil

De echte SDK initialiseert `gx->tref[i]` met het register-ID in de bovenste byte:
```
tref[0] = 0x28000000    (register 0x28)
tref[1] = 0x29000000    (register 0x29)
...
tref[7] = 0x2F000000    (register 0x2F)
```

Onze port initialiseert `gc_gx_tref[i] = 0` (zonder register-ID).

Twee opties om dit op te lossen:
1. **Port aanpassen**: `gc_gx_tref[i]` initialiseren met register-IDs (zoals de echte SDK)
2. **Bij vergelijking strippen**: alleen de onderste 24 bits vergelijken

Optie 1 is beter — dan matcht onze port exact de echte SDK representatie.

## Toepasbaar op Andere GX Functies

Elke GX functie die naar de FIFO schrijft kan op dezelfde manier getest worden:

| Functie | Opcode | Bytes per call | Notes |
|---------|--------|----------------|-------|
| GXSetTevOrder | 0x61 (BP) | 5 | 1 BP register write |
| GXSetTevOp | 0x61 (BP) | 10 | 2 BP writes (color + alpha) |
| GXSetTevColorIn | 0x61 (BP) | 5 | 1 BP write |
| GXSetBlendMode | 0x61 (BP) | 5 | 1 BP write |
| GXLoadPosMtxImm | 0x10 (XF) | varies | XF register load |
| GXSetViewport | 0x10 (XF) | varies | XF register load |
| GXBegin | 0x80-0x9F | 3 | Primitive header |

De BP (Blitting Processor) functies zijn het makkelijkst: altijd `0x61 + 4 bytes`.
XF (Transform) en CP (Command Processor) functies hebben andere formaten.

## Stappen

1. Bouw DOL die `GXRedirectWriteGatherPipe` gebruikt om FIFO te capturen
2. Draai in Dolphin, dump capture buffer
3. Pas port aan: voeg register-IDs toe aan tref[] (en andere BP registers)
4. Bouw host scenario dat verwachte FIFO bytes construeert
5. Vergelijk: byte-identical = PASS
6. Herhaal voor andere GX functies
