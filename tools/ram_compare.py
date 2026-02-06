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


def compare_ranges(expected: bytes, actual: bytes, ranges, max_diffs: int):
    diffs = []
    for start, length in ranges:
        if length <= 0:
            continue
        end = start + length
        if start < 0:
            continue
        # Clamp to file length.
        end = min(end, len(expected), len(actual))
        if start >= end:
            continue
        for i in range(start, end):
            if expected[i] != actual[i]:
                diffs.append((i, expected[i], actual[i]))
                if len(diffs) >= max_diffs:
                    return diffs
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
    parser.add_argument("--ignore-prefix", type=lambda s: int(s, 0), default=0,
                        help="ignore the first N bytes (useful for low-memory/loader noise)")
    parser.add_argument(
        "--include-range",
        action="append",
        default=[],
        help="only compare a range: <offset>,<len> (hex ok). Can be repeated.",
    )
    args = parser.parse_args()

    expected = read_file(args.expected)
    actual = read_file(args.actual)

    if len(expected) != len(actual):
        print(f"Size mismatch: expected {len(expected)} bytes, actual {len(actual)} bytes")

    if args.include_range:
        ranges = []
        for spec in args.include_range:
            try:
                off_s, len_s = spec.split(",", 1)
                off = int(off_s, 0)
                ln = int(len_s, 0)
                ranges.append((off, ln))
            except Exception:
                print(f"Bad --include-range {spec!r} (want <offset>,<len>)")
                sys.exit(2)
        diffs = compare_ranges(expected, actual, ranges, args.max_diffs)
        base_off = 0
        exp_view = expected
        act_view = actual
    else:
        # Compare byte-by-byte, optionally skipping a prefix that is known to be
        # loader/exception-vector noise in some Dolphin boot flows.
        if args.ignore_prefix > 0:
            exp_view = expected[args.ignore_prefix:]
            act_view = actual[args.ignore_prefix:]
            base_off = args.ignore_prefix
        else:
            exp_view = expected
            act_view = actual
            base_off = 0

        diffs = compare_bytes(exp_view, act_view, args.max_diffs, args.context)

    if not diffs and len(expected) == len(actual):
        print("PASS: files are identical")
        sys.exit(0)

    if not diffs:
        print("FAIL: size mismatch only")
        sys.exit(1)

    print(f"FAIL: {len(diffs)} diffs (showing up to {args.max_diffs})")
    for off, exp_b, act_b in diffs:
        real_off = off + base_off
        print(f"- offset 0x{real_off:08x}: expected {exp_b:02x}, actual {act_b:02x}")
        start = max(0, off - args.context)
        print(f"  expected: {format_hex_block(exp_view, start)}")
        print(f"  actual  : {format_hex_block(act_view, start)}")

    sys.exit(1)


if __name__ == "__main__":
    main()
