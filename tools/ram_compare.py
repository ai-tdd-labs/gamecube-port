#!/usr/bin/env python3
import argparse
import sys
from pathlib import Path


def read_file(path: Path) -> bytes:
    try:
        return path.read_bytes()
    except Exception as e:
        print(f"Error reading {path}: {e}")
        sys.exit(2)


def compare_bytes(expected: bytes, actual: bytes, max_diffs: int, chunk: int):
    diffs = []
    min_len = min(len(expected), len(actual))

    for i in range(min_len):
        if expected[i] != actual[i]:
            diffs.append((i, expected[i], actual[i]))
            if len(diffs) >= max_diffs:
                break

    return diffs


def format_hex_block(data: bytes, offset: int, width: int = 16) -> str:
    chunk = data[offset:offset + width]
    hexs = " ".join(f"{b:02x}" for b in chunk)
    return f"0x{offset:08x}: {hexs}"


def main():
    parser = argparse.ArgumentParser(description="Compare expected vs actual RAM dumps")
    parser.add_argument("expected", type=Path, help="expected.bin")
    parser.add_argument("actual", type=Path, help="actual.bin")
    parser.add_argument("--max-diffs", type=int, default=8, help="max diffs to show")
    parser.add_argument("--context", type=int, default=16, help="bytes of context to print around diff")
    args = parser.parse_args()

    expected = read_file(args.expected)
    actual = read_file(args.actual)

    if len(expected) != len(actual):
        print(f"Size mismatch: expected {len(expected)} bytes, actual {len(actual)} bytes")

    diffs = compare_bytes(expected, actual, args.max_diffs, args.context)

    if not diffs and len(expected) == len(actual):
        print("PASS: files are identical")
        sys.exit(0)

    if not diffs:
        print("FAIL: size mismatch only")
        sys.exit(1)

    print(f"FAIL: {len(diffs)} diffs (showing up to {args.max_diffs})")
    for off, exp_b, act_b in diffs:
        print(f"- offset 0x{off:08x}: expected {exp_b:02x}, actual {act_b:02x}")
        start = max(0, off - args.context)
        print(f"  expected: {format_hex_block(expected, start)}")
        print(f"  actual  : {format_hex_block(actual, start)}")

    sys.exit(1)


if __name__ == "__main__":
    main()
