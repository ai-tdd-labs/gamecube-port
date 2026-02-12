# DOL PBT Workflow — Stap-voor-stap handleiding

Hoe je een DOL Property-Based Test opzet voor een SDK port functie.
Bedoeld als handleiding zodat elk LLM-model dit proces kan herhalen voor andere functies.

## Overzicht

Een DOL PBT test draait dezelfde functie-aanroepen op twee platformen:
- **DOL** (PowerPC): gecompileerd met devkitPPC, draait in Dolphin emulator
- **Host** (x86): gecompileerd met clang/cc, draait native op de host machine

Beide dumpen hun output als big-endian bytes naar dezelfde geheugenregio (0x80300000).
Als de binaries byte-identical zijn → PASS.

Er zijn twee niveaus:
1. **Cross-platform PBT**: port code op PPC vs port code op x86 (vangt compiler/platform bugs)
2. **FIFO capture PBT**: echte libogc SDK op PPC vs port code op x86 (vangt decomp/logica fouten)

Dit document beschrijft niveau 1. Zie `GX_FIFO_PBT.md` voor niveau 2.

## Stap 1: Trace Harvest (optioneel maar aanbevolen)

Harvest eerst echte aanroepen uit de retail game om te begrijpen welke inputs de game gebruikt.

```bash
cd gamecube-port

python tools/trace_pc_entry_exit.py \
  --rvz "/c/Users/chris/Downloads/Mario Party 4 (USA).rvz" \
  --entry-pc 0x800CE6BC \
  --dump "sdk_state:0x817FE000:0x2000" \
  --max-hits 20 \
  --out-dir tests/trace-harvest/gx_set_tev_order/mp4_rvz \
  --dolphin "C:/projects/dolphin-gdb/Binary/x64/Dolphin.exe"
```

**Wat je hieruit haalt:**
- Echte register waarden (in_regs.json / out_regs.json per hit)
- RAM state snapshots (in_sdk_state.bin / out_sdk_state.bin)
- Patronen: welke input-combinaties komen voor in de echte game

**Hoe je het entry-pc adres vindt:**
- Zoek in `external/mp4-decomp/config/GMPE01_00/symbols.txt`
- Formaat: `adres:size funcnaam`
- Voorbeeld: `0x800CE6BC:0x1DC GXSetTevOrder`

## Stap 2: Decomp Lezen

Lees de originele decompilatie om de functie volledig te begrijpen.

**Locatie:** `external/mp4-decomp/src/dolphin/<subsystem>/<file>.c`

**Wat je moet noteren:**
- **Input validatie** (ASSERTMSGLINE regels) → bepaalt geldige input ranges
- **Observable state** → welke struct fields / globals worden gewijzigd
- **Bitfield layout** → SET_REG_FIELD(line, reg, size, shift, value)
- **Lookup tables** → static arrays zoals c2r[]
- **FIFO writes** → GX_WRITE_RAS_REG, GX_WRITE_XF_REG, etc.
- **Speciale gevallen** → NULL checks, boundary values

Voorbeeld voor GXSetTevOrder:
```
Assertions:
  stage < 16
  coord < 8 || coord == 0xFF
  (map & ~0x100) < 8 || map == 0xFF
  color >= 4 && color <= 0xFF

Observable state:
  gx->tref[stage/2]     (bitfield, even/odd halven)
  gx->texmapId[stage]   (direct assign)
  gx->tevTcEnab          (bitwise AND/OR op bit 'stage')
  gx->bpSentNot          (set to 0)
  gx->dirtyState         (OR met 1)
```

## Stap 3: Port Code Lezen

Lees de port implementatie in `src/sdk_port/<subsystem>/`.

**Wat je moet controleren:**
- Welke globale variabelen corresponderen met de decomp's struct fields
- Of de logica identiek is aan de decomp
- Welke variabelen `extern` beschikbaar zijn voor de host scenario

Voorbeeld mapping:
```
Decomp                    Port
------                    ----
gx->tref[i]           → gc_gx_tref[i]
gx->texmapId[i]       → gc_gx_texmap_id[i]
gx->tevTcEnab         → gc_gx_tev_tc_enab
gx->bpSentNot         → gc_gx_bp_sent_not
gx->dirtyState        → gc_gx_dirty_state
```

## Stap 4: Input Space Bepalen

Gebruik de decomp assertions om alle geldige inputs te bepalen.

```
stage  : 0-15              (16 waarden)
coord  : 0-7, 0xFF         (9 waarden)
map    : 0-7, 0x100-0x107, 0xFF  (17 waarden)
color  : 4-8, 0xFF         (6 waarden)

Totaal: 16 × 9 × 17 × 6 = 14688 combinaties
```

**Vuistregel:**
- < 100K combinaties → exhaustive test (alle combinaties)
- \> 100K combinaties → PRNG-based sampling (vaste seed voor reproduceerbaarheid)

## Stap 4b: Test Levels Ontwerpen

Een enkele exhaustive sweep vangt niet alles. Ontwerp meerdere test levels:

| Level | Naam | Wat het test | Hoe |
|-------|------|-------------|-----|
| **L0** | Isolated | Basale correctheid per combinatie | Reset → 1 call → hash (exhaustive) |
| **L1** | Accumulation | Meerdere calls bewaren elkaars bits | Reset → 2+ calls op gerelateerde state → hash |
| **L2** | Overwrite | Overschrijven met nieuwe params | Reset → call → call zelfde slot → hash |
| **L3** | Random start | set_field raakt alleen target bits | Random state → 1 call → hash |
| **L4** | Harvest replay | Echte game sequentie | Reset → N harvested calls → hash |
| **L5** | Boundary | Edge cases en noop checks | Ongeldige stage, max waarden, 0xFF params |

**L0** is altijd verplicht. De andere levels zijn optioneel maar sterk aanbevolen.
Voor L1-L3 gebruik een PRNG (xorshift32) met vaste seed voor reproduceerbaarheid.

### PRNG Template (xorshift32)

```c
static u32 rng_state;

static u32 rng_next(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}
```

Gebruik unieke seeds per level: bijv. L1=0xDEADBEEF, L2=0xCAFEBABE, L3=0x13377331.

## Stap 5: DOL Source Schrijven

Locatie: `tests/sdk/<subsystem>/<functie>/dol/pbt/<naam>/<naam>.c`

### Template

```c
#include <stdint.h>
typedef uint32_t u32;
typedef uint8_t  u8;

/* ---- Self-contained kopie van de functie (uit port/decomp) ---- */

static u32 s_state_var1[N];
static u32 s_state_var2;
// ... alle observable state als static locals ...

static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

static void FunctieOnderTest(u32 arg1, u32 arg2, ...) {
    // Exacte kopie van de port code, maar met s_ locals ipv gc_ globals
}

/* ---- State reset ---- */

static void reset_state(void) {
    // Zet alle observable state naar bekende beginwaarden
    // Dit is cruciaal: elke test case moet geïsoleerd zijn
}

/* ---- Output helpers ---- */

static inline void wr32be(volatile u8 *p, u32 v) {
    p[0]=(u8)(v>>24); p[1]=(u8)(v>>16); p[2]=(u8)(v>>8); p[3]=(u8)v;
}

static inline u32 hash_state(void) {
    // XOR-rotate over alle observable state
    u32 h = 0;
    for (int i = 0; i < N; i++)
        h = (h << 1 | h >> 31) ^ s_state_var1[i];
    h = (h << 1 | h >> 31) ^ s_state_var2;
    return h;
}

static void dump_full_state(volatile u8 *p, u32 *off) {
    // Dump alle observable state als big-endian u32's
    for (int i = 0; i < N; i++) { wr32be(p + *off, s_state_var1[i]); *off += 4; }
    wr32be(p + *off, s_state_var2); *off += 4;
}

/* ---- Input arrays ---- */

static const u32 arg1_vals[] = { ... };
static const u32 arg2_vals[] = { ... };
#define N_ARG1 ...
#define N_ARG2 ...

/* ---- Output layout ----
 *  [0x000] u32 magic = 0xBEEF####
 *  [0x004] u32 num_cases
 *  [0x008] u32 hash_all
 *  [0x00C] u32 hash_per_group[G]  (G × 4 bytes)
 *  [0x0XX] spot-check full state dump (voor debugging bij mismatch)
 */

int main(void) {
    volatile u8 *out = (volatile u8 *)0x80300000;
    u32 i;
    for (i = 0; i < OUTPUT_SIZE; i++) out[i] = 0;

    u32 hash_all = 0;
    u32 hash_group[G] = {0};
    u32 case_idx = 0;

    for (u32 a = 0; a < N_ARG1; a++) {
        for (u32 b = 0; b < N_ARG2; b++) {
            // ... meer loops voor meer args ...

            reset_state();
            FunctieOnderTest(arg1_vals[a], arg2_vals[b], ...);

            u32 h = hash_state();
            hash_all = (hash_all << 1 | hash_all >> 31) ^ h;
            hash_group[group_idx] = (hash_group[group_idx] << 1 | hash_group[group_idx] >> 31) ^ h;
            case_idx++;
        }
    }

    /* Write output */
    u32 off = 0;
    wr32be(out + off, MAGIC); off += 4;
    wr32be(out + off, case_idx); off += 4;
    wr32be(out + off, hash_all); off += 4;
    for (i = 0; i < G; i++) { wr32be(out + off, hash_group[i]); off += 4; }

    /* Spot check: 1 specifieke case met volledige state dump */
    reset_state();
    FunctieOnderTest(SPOT_ARG1, SPOT_ARG2, ...);
    dump_full_state(out, &off);

    while (1) { __asm__ volatile("nop"); }
    return 0;
}
```

### Waarom self-contained kopie?

De DOL bevat een KOPIE van de port code (met static locals) in plaats van de oracle include
(`#include "src/sdk_port/gx/GX.c"`). Reden: het hele GX.c includen trekt gc_mem en andere
dependencies mee die niet linken in een DOL. De self-contained kopie vermijdt dit.

De host scenario gebruikt WEL de echte port code (extern globals). Zo test je:
- Of de port logica consistent is tussen PPC en x86
- Of de self-contained kopie matcht met de port (copy-paste bugs vangen)

## Stap 6: Makefile

Locatie: zelfde directory als de DOL source.

```makefile
# <naam> SDK DOL test build
TOPDIR ?= $(CURDIR)
REPO_ROOT := $(shell git -C "$(TOPDIR)" rev-parse --show-toplevel)

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment.")
endif

export DEVKITPPC := $(DEVKITPRO)/devkitPPC
include $(DEVKITPRO)/devkitPPC/gamecube_rules

TARGET := <naam>
BUILD := build
SOURCES := .

INCLUDE := -I$(LIBOGC_INC) -I$(REPO_ROOT)
CFLAGS = -g -O2 -Wall -std=gnu99 $(MACHDEP) $(INCLUDE)
LDFLAGS = -g $(MACHDEP) -Wl,-Map,$(notdir $@).map

LIBS := -logc
LIBDIRS := $(LIBOGC_LIB)
LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir))

ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(TOPDIR)/$(TARGET)
export VPATH := $(foreach dir,$(SOURCES),$(TOPDIR)/$(dir))
export DEPSDIR := $(TOPDIR)/$(BUILD)
CFILES := $(notdir $(wildcard $(TOPDIR)/*.c))
export LD := $(CC)
export OFILES := $(CFILES:.c=.o)
.PHONY: $(BUILD) clean
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(TOPDIR)/Makefile TOPDIR=$(TOPDIR)
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(OUTPUT).dol $(OUTPUT).elf $(OUTPUT).dol.map
else
DEPENDS := $(OFILES:.o=.d)
$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)
-include $(DEPENDS)
endif
```

## Stap 7: Host Scenario Schrijven

Locatie: `tests/sdk/<subsystem>/<functie>/host/<naam>_scenario.c`

### Template

```c
#include <stdint.h>
#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef uint8_t  u8;

/* Port globals (extern uit sdk_port) */
extern u32 gc_state_var1[N];
extern u32 gc_state_var2;

void FunctieOnderTest(u32 arg1, u32 arg2, ...);

/* Input arrays — MOET exact matchen met DOL */
static const u32 arg1_vals[] = { ... };
// ...

static void reset_state(void) { /* zelfde als DOL maar met gc_ globals */ }
static u32 hash_state(void) { /* zelfde als DOL maar met gc_ globals */ }
static void dump_full_state(u8 *p, u32 *off) { /* zelfde maar met gc_ globals en wr32be() */ }

const char *gc_scenario_label(void) { return "Functie/pbt"; }
const char *gc_scenario_out_path(void) { return "../actual/<naam>.bin"; }

void gc_scenario_run(GcRam *ram) {
    u8 *out = gc_ram_ptr(ram, 0x80300000u, OUTPUT_SIZE);
    if (!out) die("gc_ram_ptr failed");

    // EXACT DEZELFDE LOGICA ALS DOL main()
    // Zelfde loops, zelfde volgorde, zelfde hash berekening
    // Enige verschil: gc_ globals ipv s_ locals, wr32be() ipv volatile writes
}
```

**Kritiek: de host scenario MOET exact dezelfde test volgorde, reset logica, en output
layout hebben als de DOL. Elk verschil → mismatch → false failure.**

## Stap 8: Bouwen

### DOL (devkitPPC)

Op Windows/MSYS2 heeft Make permission issues met temp directories.
Bouw handmatig:

```bash
DEVKITPPC=/c/devkitpro/devkitPPC
REPO=D:/projects/ps2/crash_bandicoot/reference/gamecube-port
DOL_DIR=$REPO/tests/sdk/gx/gx_set_tev_order/dol/pbt/gx_set_tev_order_pbt_001

# Compile
$DEVKITPPC/bin/powerpc-eabi-gcc \
  -O2 -Wall -std=gnu99 -mogc -mcpu=750 -meabi -mhard-float \
  -I/c/devkitpro/libogc/include -I"$REPO" \
  -c "$DOL_DIR/<naam>.c" -o "$DOL_DIR/<naam>.o"

# Link
$DEVKITPPC/bin/powerpc-eabi-gcc \
  "$DOL_DIR/<naam>.o" \
  -L/c/devkitpro/libogc/lib/cube -logc \
  -o "$DOL_DIR/<naam>.elf" -mogc

# Convert to DOL
/c/devkitpro/tools/bin/elf2dol "$DOL_DIR/<naam>.elf" "$DOL_DIR/<naam>.dol"
```

### Host scenario

```bash
cd $REPO

# Standaard (dump 0x40 bytes):
tools/run_host_scenario.sh tests/sdk/<sub>/<func>/host/<naam>_scenario.c

# Met aangepaste dump grootte:
GC_HOST_MAIN_DUMP_SIZE=0xC0 tools/run_host_scenario.sh tests/sdk/<sub>/<func>/host/<naam>_scenario.c
```

**Let op: GC_HOST_MAIN_DUMP_SIZE moet >= je output grootte zijn, anders wordt de output
afgekapt en faalt de vergelijking op grootte.**

## Stap 9: Draaien in Dolphin

```bash
python tools/ram_dump.py \
  --exec "$DOL_DIR/<naam>.dol" \
  --enable-mmu \
  --addr 0x80300000 \
  --size 0xC0 \
  --out "$DOL_DIR/expected/<naam>.bin" \
  --run 3.0 \
  --halt \
  --dolphin "C:/projects/dolphin-gdb/Binary/x64/Dolphin.exe"
```

**Parameters:**
- `--run 3.0` → laat DOL 3 seconden draaien (genoeg voor ~15K iteraties)
- `--halt` → stop emulatie na timeout
- `--size 0xC0` → MOET matchen met je output layout grootte
- `--enable-mmu` → nodig voor correcte adres vertaling

## Stap 10: Vergelijken

```bash
EXPECTED=$DOL_DIR/expected/<naam>.bin
ACTUAL=$REPO/tests/sdk/<sub>/<func>/actual/<naam>.bin

cmp "$EXPECTED" "$ACTUAL" && echo "PASS" || echo "FAIL"
```

Bij FAIL: gebruik het Python script om de binaries te parsen en het eerste verschil te vinden:

```python
import struct
with open(expected, 'rb') as f: exp = f.read()
with open(actual, 'rb') as f: act = f.read()
for i in range(0, min(len(exp), len(act)), 4):
    e = struct.unpack('>I', exp[i:i+4])[0]
    a = struct.unpack('>I', act[i:i+4])[0]
    if e != a:
        print(f"MISMATCH at offset 0x{i:04X}: expected 0x{e:08X}, actual 0x{a:08X}")
        break
```

## Checklist voor Nieuwe Functies

- [ ] Symbool adres gevonden in symbols.txt
- [ ] Decomp gelezen, assertions genoteerd
- [ ] Port code gelezen, observable state geïdentificeerd
- [ ] Input space bepaald (exhaustive of PRNG)
- [ ] DOL source geschreven met self-contained functie kopie
- [ ] Input arrays en hash logica identiek in DOL en host
- [ ] Makefile aangemaakt (kopieer van bestaande test)
- [ ] Host scenario geschreven met extern port globals
- [ ] DOL gebouwd (gcc → elf → dol)
- [ ] DOL gedraaid in Dolphin, expected.bin gedumt
- [ ] Host scenario gedraaid, actual.bin gedumt
- [ ] GC_HOST_MAIN_DUMP_SIZE correct ingesteld
- [ ] `cmp expected.bin actual.bin` → PASS

## Veelvoorkomende Fouten

| Probleem | Oorzaak | Oplossing |
|----------|---------|-----------|
| Bestanden verschillen in grootte | GC_HOST_MAIN_DUMP_SIZE te klein | Stel in op >= output grootte |
| Make faalt met "Permission denied" | Windows temp dir permission | Bouw handmatig met gcc → elf2dol |
| `powerpc-eabi-gcc` not found | PATH mist devkitPPC/bin | Gebruik volledig pad of voeg toe aan PATH |
| DOL output is allemaal nullen | DOL klaar voor timeout | Verhoog --run (bijv 5.0) |
| Hash mismatch bij 1 stage | Reset state onvolledig | Check of ALLE observable vars gereset worden |
| L0 PASS maar L1/L2/L3 FAIL | rng_next() in functie args | Arg evaluatie volgorde is ongespecificeerd in C! Extract rng_next() naar lokale variabelen vóór de functie call (zie hieronder) |
| Host linker waarschuwing LNK4044 | MSVC negeert --gc-sections | Negeren, is onschadelijk |

### Kritieke Valkuil: Argument Evaluatie Volgorde

**NOOIT** meerdere `rng_next()` calls in dezelfde functie-argumenten zetten:

```c
// FOUT — evaluatie volgorde ongespecificeerd, PPC≠x86!
FuncOnderTest(stage, vals[rng_next() % N], vals[rng_next() % N], vals[rng_next() % N]);

// GOED — extract naar lokale variabelen
u32 a = rng_next() % N;
u32 b = rng_next() % N;
u32 c = rng_next() % N;
FuncOnderTest(stage, vals[a], vals[b], vals[c]);
```

C garandeert GEEN volgorde van evaluatie van functie-argumenten. PPC GCC kan left-to-right
evalueren terwijl x86 Clang right-to-left evalueert. Dit geeft een andere PRNG sequentie
op de twee platformen, waardoor de hashes niet matchen — zelfs als de functie logica correct is.

## Directory Structuur

```
tests/sdk/<subsystem>/<functie>/
├── dol/
│   └── pbt/
│       └── <naam>/
│           ├── <naam>.c              ← DOL source
│           ├── Makefile
│           ├── <naam>.o              ← compiled object (gegenereerd)
│           ├── <naam>.elf            ← linked ELF (gegenereerd)
│           ├── <naam>.dol            ← final DOL (gegenereerd)
│           └── expected/
│               └── <naam>.bin        ← Dolphin output (gegenereerd)
├── host/
│   └── <naam>_scenario.c            ← host scenario
└── actual/
    └── <naam>.bin                    ← host output (gegenereerd)
```

## Referentie: Bestaande DOL PBT

Eerste werkende voorbeeld: `tests/sdk/gx/gx_set_tev_order/dol/pbt/gx_set_tev_order_pbt_001/`
- 6 test levels, 24710 totale cases
  - L0 Isolated: 14688 exhaustive
  - L1 Accumulation: 8000 random (seed 0xDEADBEEF)
  - L2 Overwrite: 1000 random (seed 0xCAFEBABE)
  - L3 Random start: 1000 random (seed 0x13377331)
  - L4 Harvest replay: 15 real game calls
  - L5 Boundary: 7 edge cases
- Hash-based output (512 bytes, magic 0xBEEF0002)
- PASS op Windows/MSYS2 met Dolphin GDB build
