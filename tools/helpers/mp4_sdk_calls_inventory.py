#!/usr/bin/env python3
"""
Generate an inventory of Nintendo SDK-style calls made by the MP4 workload slice.

This is "repo memory": it answers "what SDK calls does MP4 make?" using evidence
from the decompiled MP4 vendor slice we keep under src/game_workload/mp4/vendor.

Output: docs/sdk/mp4/MP4_sdk_calls_inventory.csv
"""

from __future__ import annotations

import csv
import re
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
MP4_SRC_DIR = REPO_ROOT / "src/game_workload/mp4/vendor/src/game"
CHAIN_ALL_CSV = REPO_ROOT / "docs/sdk/mp4/MP4_chain_all.csv"
TESTS_SDK_DIR = REPO_ROOT / "tests/sdk"
OUT_CSV = REPO_ROOT / "docs/sdk/mp4/MP4_sdk_calls_inventory.csv"


CALL_RE = re.compile(r"\b(OS|GX|VI|PAD|DVD|SI|EXI)([A-Za-z0-9_]*)\s*\(")
PREFIX_TO_SUBSYSTEM = {
    "OS": "os",
    "GX": "gx",
    "VI": "vi",
    "PAD": "pad",
    "DVD": "dvd",
    "SI": "si",
    "EXI": "exi",
}


def rest_to_snake(rest: str) -> str:
    """Convert the post-prefix part to snake_case.

    Handles transitions:
    - lower->upper (fooBar)
    - alpha<->digit (Up32B)
    """
    if not rest:
        return ""
    s = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", rest)
    s = re.sub(r"([A-Za-z])([0-9])", r"\1_\2", s)
    s = re.sub(r"([0-9])([A-Za-z])", r"\1_\2", s)
    return s.lower()


def sdk_name_to_suite_dir(prefix: str, rest: str) -> str:
    # E.g. ("PAD","Read") -> "pad_read"
    #
    # Some GX immediate-mode helpers are conventionally named without underscores
    # around the signedness/bitwidth chunk (e.g. gx_position_3s16, not
    # gx_position_3_s_16). Mirror the existing suite naming so we can find
    # coverage for MP4 callsite-style tests.
    if prefix == "GX":
        m = re.fullmatch(r"Position([0-9]+)([su])([0-9]+)", rest)
        if m:
            n, t, bits = m.group(1), m.group(2), m.group(3)
            return f"gx_position_{n}{t}{bits}"
    return f"{prefix.lower()}_{rest_to_snake(rest)}" if rest else prefix.lower()


def load_chain_all_functions() -> dict[str, str]:
    """Return function -> canonical_suite for MP4_chain_all.csv."""
    fn_to_suite: dict[str, str] = {}
    with CHAIN_ALL_CSV.open(newline="", encoding="utf-8") as f:
        r = csv.DictReader(f)
        for row in r:
            fn = row.get("function", "").strip()
            suite = row.get("canonical_suite", "").strip()
            if fn:
                fn_to_suite.setdefault(fn, suite)
    return fn_to_suite


def has_test_suite(prefix: str, rest: str) -> tuple[bool, str]:
    """Best-effort check for a tests/sdk/<subsystem>/<suite_dir> folder."""
    subsystem = PREFIX_TO_SUBSYSTEM.get(prefix)
    if not subsystem:
        return False, ""
    suite = sdk_name_to_suite_dir(prefix, rest)
    p = TESTS_SDK_DIR / subsystem / suite
    return p.is_dir(), str(p.relative_to(REPO_ROOT)) if p.is_dir() else ""


@dataclass(frozen=True)
class FnInfo:
    fn: str
    normalized: str
    kind: str
    callsites: int
    files: str
    in_chain_all: str
    canonical_suite: str
    has_test_suite: str
    notes: str


def main() -> int:
    if not MP4_SRC_DIR.exists():
        raise SystemExit(f"MP4 src dir not found: {MP4_SRC_DIR}")
    if not CHAIN_ALL_CSV.exists():
        raise SystemExit(f"Chain csv not found: {CHAIN_ALL_CSV}")

    chain_fn_to_suite = load_chain_all_functions()

    # Collect known function-like macros from our vendored headers so we don't
    # accidentally treat them as SDK functions that need porting.
    macro_names: set[str] = set()
    for hdr in (REPO_ROOT / "tests/workload/include").rglob("*.h"):
        try:
            txt = hdr.read_text(encoding="utf-8", errors="replace")
        except Exception:
            continue
        for m in re.finditer(r"^\s*#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(", txt, flags=re.M):
            macro_names.add(m.group(1))

    fn_to_files: dict[str, set[str]] = defaultdict(set)
    fn_to_count: dict[str, int] = defaultdict(int)
    fn_to_parts: dict[str, tuple[str, str]] = {}

    files = sorted(MP4_SRC_DIR.glob("*.c"))
    for fp in files:
        text = fp.read_text(encoding="utf-8", errors="replace")
        for m in CALL_RE.finditer(text):
            prefix = m.group(1)
            rest = m.group(2)
            fn = f"{prefix}{rest}"
            fn_to_parts.setdefault(fn, (prefix, rest))
            fn_to_files[fn].add(fp.name)
            fn_to_count[fn] += 1

    out: list[FnInfo] = []
    for fn in sorted(fn_to_count.keys()):
        prefix, rest = fn_to_parts.get(fn, ("", fn))
        normalized = sdk_name_to_suite_dir(prefix, rest) if prefix else rest_to_snake(fn)
        kind = "macro" if fn in macro_names else "fn"
        in_chain = "y" if fn in chain_fn_to_suite else "n"
        canonical_suite = chain_fn_to_suite.get(fn, "")
        exists, suite_path = has_test_suite(prefix, rest)
        suite_ref = canonical_suite or suite_path
        if kind == "macro":
            has_suite = "n/a"
        else:
            has_suite = "y" if suite_ref else "n"
        out.append(
            FnInfo(
                fn=fn,
                normalized=normalized,
                kind=kind,
                callsites=fn_to_count[fn],
                files=";".join(sorted(fn_to_files[fn])),
                in_chain_all=in_chain,
                canonical_suite=suite_ref,
                has_test_suite=has_suite,
                notes="",
            )
        )

    OUT_CSV.parent.mkdir(parents=True, exist_ok=True)
    with OUT_CSV.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(
            [
                "function",
                "normalized",
                "kind",
                "callsites_count",
                "files",
                "in_chain_all",
                "canonical_suite",
                "has_test_suite",
                "notes",
            ]
        )
        for row in out:
            w.writerow(
                [
                    row.fn,
                    row.normalized,
                    row.kind,
                    row.callsites,
                    row.files,
                    row.in_chain_all,
                    row.canonical_suite,
                    row.has_test_suite,
                    row.notes,
                ]
            )

    print(f"Wrote: {OUT_CSV.relative_to(REPO_ROOT)} ({len(out)} functions)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
