---
name: dolphin-debug
description: Debug GameCube DOLs using Dolphin emulator's GDB stub for RAM inspection and testing
---
# Dolphin Debug Workflow

Use this skill to test GameCube code by running DOLs in Dolphin and inspecting RAM via GDB stub.

## Prerequisites

- Dolphin installed at `/Applications/Dolphin.app/`
- Python 3 with socket module
- A DOL file to test

## Starting Dolphin with GDB Stub

```bash
# Start Dolphin with debugger enabled (-d flag)
/Applications/Dolphin.app/Contents/MacOS/Dolphin -d -e /path/to/file.dol &
```

Flags:
- `-d`: Enable GDB stub on port 9090
- `-e`: Execute the specified DOL/ISO
- `-b`: (optional) Batch/headless mode - use only if no GUI needed

**Important:** Without `-b`, Dolphin GUI stays open and GDB stub remains active.

## GDB Stub Connection Workflow

The GDB stub only responds when CPU is halted. The correct workflow is:

1. **Connect** to `127.0.0.1:9090`
2. **Continue** execution (`c` command) - lets DOL code run
3. **Wait** for code to execute (e.g., 0.5s)
4. **Halt** CPU (send `0x03` byte)
5. **Read** memory or registers

## Using ram_dump.py

```bash
# Dump RAM after DOL has run for 0.5 seconds
python3 tools/ram_dump.py \
    --addr 0x80300000 \
    --size 256 \
    --out output.bin \
    --run 0.5

# With auto-start Dolphin
python3 tools/ram_dump.py \
    --exec /path/to/test.dol \
    --addr 0x80300000 \
    --size 256 \
    --out output.bin \
    --delay 3 \
    --run 0.5
```

Options:
- `--addr`: Start address (hex ok, e.g., `0x80300000`)
- `--size`: Bytes to read
- `--out`: Output file path
- `--run N`: Run emulation for N seconds then halt (REQUIRED for DOLs that write data)
- `--halt`: Just halt without running first
- `--exec`: Path to DOL to auto-start
- `--delay`: Seconds to wait after starting Dolphin (default: 2)

## Test DOL Pattern

The test DOL at `tools/test_dol/loop.dol` writes this pattern to `0x80300000`:

| Offset | Size | Content |
|--------|------|---------|
| 0x00 | 64 bytes | `0xDEADBEEF` repeated |
| 0x40 | 64 bytes | `0xCAFEBABE` repeated |
| 0x80 | 128 bytes | Sequential `0x00` to `0x7F` |

Use this to verify the debug workflow is working.

## Example: Verify Test Pattern

```bash
# Start Dolphin with test DOL
/Applications/Dolphin.app/Contents/MacOS/Dolphin -d -e tools/test_dol/loop.dol &
sleep 2

# Dump and verify
python3 tools/ram_dump.py --addr 0x80300000 --size 256 --out /tmp/test.bin --run 0.5
xxd /tmp/test.bin | head -10
# Should show: DEADBEEF, CAFEBABE, then 00 01 02 03...
```

## Common Issues

**GDB stub not accessible:**
- Check Dolphin is running with `-d` flag
- Don't use `nc -z` to test - it closes the single-connection stub
- Port 9090 must be free

**Memory reads return zeros:**
- DOL hasn't executed yet
- Use `--run 0.5` or longer to let code run before halting

**Connection refused:**
- Previous connection still held - kill Dolphin and restart
- GDB stub only accepts one connection at a time

## Sources

- GDB protocol: memory read = `m<addr>,<size>`
- Dolphin source: `Source/Core/Core/Debugger/GDBStub.cpp`
