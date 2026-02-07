#!/usr/bin/env python3
import csv
import re
import sys
from pathlib import Path


SDK_PREFIXES = (
    "OS",
    "VI",
    "GX",
    "DVD",
    "PAD",
    "SI",
    "EXI",
    "AI",
    "AR",
    "DSP",
    "CARD",
)


def extract_calls(text: str) -> list[str]:
    # Heuristic: capture FooBar( tokens and filter by known SDK prefixes.
    # This is intentionally conservative; it's a "what should we look at next" probe.
    calls = []
    for m in re.finditer(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(", text):
        name = m.group(1)
        if name.startswith(SDK_PREFIXES):
            calls.append(name)
    # Preserve first-seen order, dedupe.
    seen = set()
    out = []
    for c in calls:
        if c in seen:
            continue
        seen.add(c)
        out.append(c)
    return out


def guess_subsystem(name: str) -> str:
    for p in SDK_PREFIXES:
        if name.startswith(p):
            return p.lower()
    return "sdk"


def sdk_port_symbols(repo_root: Path) -> set[str]:
    # Grep-like scan: look for function definitions in src/sdk_port/**.c.
    # This does not validate signatures.
    syms: set[str] = set()
    for cfile in (repo_root / "src/sdk_port").rglob("*.c"):
        try:
            t = cfile.read_text(errors="ignore")
        except Exception:
            continue
        for m in re.finditer(r"^\s*[A-Za-z_][A-Za-z0-9_\s\*]*\b([A-Za-z_][A-Za-z0-9_]*)\s*\(", t, re.M):
            syms.add(m.group(1))
    return syms


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: probe_mp4_init_calls.py <init.c> <out.csv>", file=sys.stderr)
        return 2

    init_c = Path(sys.argv[1])
    out_csv = Path(sys.argv[2])
    repo_root = Path(__file__).resolve().parents[2]

    text = init_c.read_text()
    calls = extract_calls(text)
    syms = sdk_port_symbols(repo_root)

    out_csv.parent.mkdir(parents=True, exist_ok=True)
    with out_csv.open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["function", "subsystem", "in_sdk_port"])
        for c in calls:
            w.writerow([c, guess_subsystem(c), "yes" if c in syms else "no"])

    print(f"Wrote {len(calls)} SDK calls to {out_csv}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

