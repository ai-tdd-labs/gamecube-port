#!/bin/bash
set -euo pipefail

if [[ -z "${DEVKITPRO:-}" ]]; then
  echo "DEVKITPRO not set. export DEVKITPRO=/opt/devkitpro" >&2
  exit 1
fi

TEST_C=${1:?"usage: build_sdk_dol.sh <test.c> <target>"}
TARGET=${2:?"usage: build_sdk_dol.sh <test.c> <target>"}

SDK_ROOT="$(cd "$(dirname "$0")/../../decomp_mario_party_4" && pwd)"
SDK_SRC="$SDK_ROOT/src/dolphin"
SDK_INC="$SDK_ROOT/include"

HARNESS_INC="$(cd "$(dirname "$0")/include" && pwd)"

PPC_GCC="$DEVKITPRO/devkitPPC/bin/powerpc-eabi-gcc"
PPC_AS="$DEVKITPRO/devkitPPC/bin/powerpc-eabi-as"
ELF2DOL="$DEVKITPRO/tools/bin/elf2dol"

OUT_DIR="$(dirname "$TEST_C")"
BUILD_DIR="$OUT_DIR/build_sdk"
mkdir -p "$BUILD_DIR"

CFLAGS_COMMON="-O2 -Wall -mcpu=750 -meabi -mhard-float -ffunction-sections -fdata-sections"
INCLUDES="-I$HARNESS_INC -I$SDK_INC"
LDFLAGS="-Wl,-gc-sections -Wl,-Map,$OUT_DIR/$TARGET.map -Wl,-Ttext=0x80003100 -Wl,-e,_start"

# Sources needed for DVDOpen/DVDClose (dvdfs) + stubs + test
SRC_FILES=(
  "$SDK_SRC/dvd/dvdfs.c"
  "$TEST_C"
  "$(dirname "$0")/os_stubs.c"
)

OBJ_FILES=()
for src in "${SRC_FILES[@]}"; do
  obj="$BUILD_DIR/$(basename "$src" .c).o"
  if [[ "$src" == *"dvdfs.c" ]]; then
    "$PPC_GCC" $CFLAGS_COMMON -std=gnu89 $INCLUDES -include ctype.h -c "$src" -o "$obj"
  else
    "$PPC_GCC" $CFLAGS_COMMON -std=gnu99 $INCLUDES -c "$src" -o "$obj"
  fi
  OBJ_FILES+=("$obj")
  done

# assemble start.s
START_O="$BUILD_DIR/start.o"
"$PPC_AS" -mgekko "$(dirname "$0")/start.s" -o "$START_O"
OBJ_FILES+=("$START_O")

ELF_OUT="$OUT_DIR/$TARGET.elf"
DOL_OUT="$OUT_DIR/$TARGET.dol"

"$PPC_GCC" $CFLAGS_COMMON ${OBJ_FILES[@]} $LDFLAGS -nostartfiles -o "$ELF_OUT"
"$ELF2DOL" "$ELF_OUT" "$DOL_OUT"

echo "Built $DOL_OUT"
