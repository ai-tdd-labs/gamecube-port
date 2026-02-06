#!/usr/bin/env python3
import argparse
from datetime import datetime, timezone
from pathlib import Path


def first_diff(expected: bytes, actual: bytes):
    min_len = min(len(expected), len(actual))
    for i in range(min_len):
        if expected[i] != actual[i]:
            return i, expected[i], actual[i]
    if len(expected) != len(actual):
        return min_len, None, None
    return None


def ensure_section(notes_text: str) -> str:
    if "## Test Runs (auto)\n" in notes_text:
        return notes_text
    if not notes_text.endswith("\n"):
        notes_text += "\n"
    notes_text += "\n## Test Runs (auto)\n\n"
    notes_text += "- Format: `[PASS|FAIL] <label> expected=<path> actual=<path> (first_mismatch=0x........)`\n"
    return notes_text


def main():
    ap = argparse.ArgumentParser(description="Record PASS/FAIL for expected.bin vs actual.bin into docs/codex/NOTES.md")
    ap.add_argument("expected", type=Path)
    ap.add_argument("actual", type=Path)
    ap.add_argument("--label", required=True, help="Short label, e.g. OSSetArenaLo/min_001")
    ap.add_argument("--notes", type=Path, default=Path("docs/codex/NOTES.md"))
    args = ap.parse_args()

    exp = args.expected.read_bytes()
    act = args.actual.read_bytes()

    d = first_diff(exp, act)
    if d is None:
        status = "PASS"
        mismatch = ""
    else:
        status = "FAIL"
        off = d[0]
        mismatch = f" first_mismatch=0x{off:08x}"

    ts = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

    notes_path = args.notes
    txt = notes_path.read_text()
    txt = ensure_section(txt)

    line = f"- [{status}] {ts} {args.label} expected={args.expected} actual={args.actual}{mismatch}\n"
    txt += line
    notes_path.write_text(txt)

    print(line.strip())
    return 0 if status == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())

