---
name: build-dol
description: Build GameCube test DOLs using devkitPPC toolchain
---
# Build Test DOLs

Use this skill to create GameCube DOL executables for testing.

## Prerequisites

- devkitPro installed at `/opt/devkitpro/`
- devkitPPC toolchain available

## Option 1: C with libogc

For DOLs that need GameCube SDK functions (VIDEO_Init, DCFlushRange, etc.)

**Directory structure:**
```
tools/test_dol/
├── Makefile
├── test_write.c
└── build/
```

**Makefile template:**
```makefile
ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment")
endif

export DEVKITPPC := $(DEVKITPRO)/devkitPPC
include $(DEVKITPRO)/devkitPPC/gamecube_rules

TARGET    := my_test
SOURCES   := .
INCLUDE   := -I$(LIBOGC_INC)
CFLAGS    = -g -O2 -Wall $(MACHDEP) $(INCLUDE)
LDFLAGS   = -g $(MACHDEP)
LIBS      := -logc
LIBDIRS   := $(LIBOGC_LIB)
LIBPATHS  := $(foreach dir,$(LIBDIRS),-L$(dir))
```

**Build command:**
```bash
export DEVKITPRO=/opt/devkitpro
export PATH=$DEVKITPRO/devkitPPC/bin:$PATH
make clean && make
```

## Option 2: Bare-metal Assembly

For minimal DOLs without libogc overhead.

**Assembly file (loop.s):**
```asm
.section .text
.global _start

_start:
    # Write pattern to 0x80300000
    lis     3, 0x8030          # r3 = 0x80300000
    lis     4, 0xDEAD
    ori     4, 4, 0xBEEF       # r4 = 0xDEADBEEF
    stw     4, 0(3)

    # Infinite loop
infinite:
    b       infinite
```

**Build script:**
```bash
#!/bin/bash
DEVKITPRO=/opt/devkitpro
AS=$DEVKITPRO/devkitPPC/bin/powerpc-eabi-as
LD=$DEVKITPRO/devkitPPC/bin/powerpc-eabi-ld
ELF2DOL=$DEVKITPRO/tools/bin/elf2dol

$AS -mgekko -o loop.o loop.s
$LD -Ttext=0x80003100 -o loop.elf loop.o
$ELF2DOL loop.elf loop.dol
```

## PPC Assembly Quick Reference

| Instruction | Description | Example |
|-------------|-------------|---------|
| `lis r, imm` | Load immediate shifted (upper 16 bits) | `lis 3, 0x8030` |
| `ori r, r, imm` | OR immediate (lower 16 bits) | `ori 3, 3, 0x1000` |
| `li r, imm` | Load immediate (signed 16-bit) | `li 4, 16` |
| `stw r, off(r)` | Store word | `stw 4, 0(3)` |
| `stb r, off(r)` | Store byte | `stb 4, 0(3)` |
| `addi r, r, imm` | Add immediate | `addi 3, 3, 4` |
| `mtctr r` | Move to count register | `mtctr 5` |
| `bdnz label` | Branch decrement not zero | `bdnz loop` |
| `b label` | Branch unconditional | `b infinite` |

## Memory Layout

| Address | Description |
|---------|-------------|
| `0x80003100` | Standard DOL entry point |
| `0x80300000` | Safe test data area |
| `0x81800000` | End of main RAM |

## Testing Your DOL

After building, test with Dolphin:
```bash
/Applications/Dolphin.app/Contents/MacOS/Dolphin -d -e your_test.dol &
sleep 2
python3 tools/ram_dump.py --addr 0x80300000 --size 64 --out test.bin --run 0.5
xxd test.bin
```
