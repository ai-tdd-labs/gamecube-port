#!/bin/bash
# Build minimal DOL from assembly
set -e

DEVKITPRO=/opt/devkitpro
AS=$DEVKITPRO/devkitPPC/bin/powerpc-eabi-as
LD=$DEVKITPRO/devkitPPC/bin/powerpc-eabi-ld
OBJCOPY=$DEVKITPRO/devkitPPC/bin/powerpc-eabi-objcopy
ELF2DOL=$DEVKITPRO/tools/bin/elf2dol

cd "$(dirname "$0")"

echo "Assembling..."
$AS -mgekko -o loop.o loop.s

echo "Linking..."
# Link at standard DOL text address
$LD -Ttext=0x80003100 -o loop.elf loop.o

echo "Converting to DOL..."
$ELF2DOL loop.elf loop.dol

echo "Done: loop.dol"
ls -la loop.dol
