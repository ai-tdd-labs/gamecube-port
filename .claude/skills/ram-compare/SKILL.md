---
name: ram-compare
description: Compare expected vs actual RAM dumps for GameCube unit testing
---
# RAM Comparison for Unit Testing

Use this skill to verify GameCube code behavior by comparing RAM dumps.

## Test Workflow

```
1. Run code in Dolphin (ground truth)
   └── Dump RAM → expected.bin

2. Run recompiled native code
   └── Dump RAM → actual.bin

3. Compare
   └── expected.bin == actual.bin → PASS
   └── expected.bin != actual.bin → FAIL (show diff)
```

## Creating Expected Output

```bash
# Run original code in Dolphin
/Applications/Dolphin.app/Contents/MacOS/Dolphin -d -e original.dol &
sleep 2

# Dump expected RAM state
python3 tools/ram_dump.py \
    --addr 0x80300000 \
    --size 4096 \
    --out tests/expected/dvd_read.bin \
    --run 1.0
```

## Quick Comparison with diff

```bash
# Binary diff
diff expected.bin actual.bin && echo "PASS" || echo "FAIL"

# Show hex differences
xxd expected.bin > /tmp/expected.hex
xxd actual.bin > /tmp/actual.hex
diff /tmp/expected.hex /tmp/actual.hex
```

## Detailed Comparison with Python

```python
#!/usr/bin/env python3
"""Compare two binary files and show differences."""

import sys

def compare_bins(expected_path, actual_path):
    with open(expected_path, 'rb') as f:
        expected = f.read()
    with open(actual_path, 'rb') as f:
        actual = f.read()

    if expected == actual:
        print(f"PASS: Files match ({len(expected)} bytes)")
        return True

    print(f"FAIL: Files differ")
    print(f"  Expected: {len(expected)} bytes")
    print(f"  Actual:   {len(actual)} bytes")

    # Find first difference
    min_len = min(len(expected), len(actual))
    for i in range(min_len):
        if expected[i] != actual[i]:
            print(f"\n  First diff at offset 0x{i:04X}:")
            print(f"    Expected: 0x{expected[i]:02X}")
            print(f"    Actual:   0x{actual[i]:02X}")

            # Show context
            start = max(0, i - 8)
            end = min(min_len, i + 8)
            print(f"\n  Context (offset 0x{start:04X}):")
            print(f"    Expected: {expected[start:end].hex()}")
            print(f"    Actual:   {actual[start:end].hex()}")
            break

    return False

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: compare.py expected.bin actual.bin")
        sys.exit(1)

    success = compare_bins(sys.argv[1], sys.argv[2])
    sys.exit(0 if success else 1)
```

## Test Directory Structure

```
tests/
├── expected/           # Dolphin ground truth
│   ├── dvd_init.bin
│   ├── dvd_read.bin
│   └── memory_alloc.bin
├── actual/             # Recompiled output
│   └── (generated during test)
└── test_dvd.py         # Test runner
```

## Example Test Runner

```python
#!/usr/bin/env python3
"""Run DVD unit tests."""

import subprocess
import sys
from pathlib import Path

TESTS_DIR = Path(__file__).parent
EXPECTED_DIR = TESTS_DIR / "expected"
ACTUAL_DIR = TESTS_DIR / "actual"

def run_test(name, dol_path, addr, size):
    """Run a single test case."""
    expected = EXPECTED_DIR / f"{name}.bin"
    actual = ACTUAL_DIR / f"{name}.bin"

    # Ensure actual dir exists
    ACTUAL_DIR.mkdir(exist_ok=True)

    # Run in Dolphin and dump
    # (In real tests, this would run recompiled native code)
    subprocess.run([
        "python3", "tools/ram_dump.py",
        "--exec", str(dol_path),
        "--addr", hex(addr),
        "--size", str(size),
        "--out", str(actual),
        "--delay", "2",
        "--run", "0.5"
    ], check=True)

    # Compare
    with open(expected, 'rb') as f:
        exp_data = f.read()
    with open(actual, 'rb') as f:
        act_data = f.read()

    if exp_data == act_data:
        print(f"✓ {name}")
        return True
    else:
        print(f"✗ {name}")
        return False

if __name__ == '__main__':
    # Define test cases
    tests = [
        ("dvd_init", "tools/test_dol/dvd_init.dol", 0x80300000, 256),
        ("dvd_read", "tools/test_dol/dvd_read.dol", 0x80300000, 4096),
    ]

    passed = sum(run_test(*t) for t in tests)
    print(f"\n{passed}/{len(tests)} tests passed")
    sys.exit(0 if passed == len(tests) else 1)
```

## Common Patterns

**Ignore volatile regions:**
Some RAM regions change between runs (timestamps, random values). Mask these before comparing:

```python
def mask_volatile(data, regions):
    """Zero out volatile regions before comparison."""
    data = bytearray(data)
    for start, end in regions:
        for i in range(start, end):
            data[i] = 0
    return bytes(data)

# Example: ignore bytes 0x10-0x1F
expected = mask_volatile(expected, [(0x10, 0x20)])
actual = mask_volatile(actual, [(0x10, 0x20)])
```

**Partial comparison:**
Sometimes only specific fields matter:

```python
# Compare only first 64 bytes
assert expected[:64] == actual[:64]

# Compare specific offset
assert expected[0x100:0x110] == actual[0x100:0x110]
```
