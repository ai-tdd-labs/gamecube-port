#!/usr/bin/env python3
import csv
import os
from pathlib import Path
import subprocess


def repo_root() -> Path:
    try:
        out = subprocess.check_output(["git", "rev-parse", "--show-toplevel"], text=True).strip()
        return Path(out)
    except Exception:
        return Path(__file__).resolve().parents[2]


def files_identical(a: Path, b: Path) -> bool:
    try:
        if a.stat().st_size != b.stat().st_size:
            return False
        # Fast path: byte compare.
        return a.read_bytes() == b.read_bytes()
    except FileNotFoundError:
        return False


def main() -> int:
    root = repo_root()
    csv_path = root / "docs/sdk/os/MP4_HuSysInit_chain_table.csv"
    if not csv_path.exists():
        raise SystemExit(f"missing: {csv_path}")

    rows = []
    with csv_path.open() as f:
        r = csv.DictReader(f)
        for row in r:
            rows.append(row)

    out_rows = []
    for row in rows:
        suite = (row.get("canonical_suite") or "").strip()
        exp_count = act_count = pass_count = 0
        covered = ""

        if suite:
            suite_path = root / suite
            exp_dir = suite_path / "expected"
            act_dir = suite_path / "actual"

            # This CSV is specifically the MP4 HuSysInit chain.
            # Only count MP4 callsite testcases (dol/mp4/*), not generic or other-game variants.
            mp4_dol_root = suite_path / "dol" / "mp4"
            mp4_cases: list[str] = []
            if mp4_dol_root.exists():
                for dol in sorted(mp4_dol_root.glob("**/*.dol")):
                    mp4_cases.append(dol.stem)

            exp_bins = []
            act_bins = []
            if mp4_cases and exp_dir.exists():
                exp_bins = [exp_dir / f"{name}.bin" for name in mp4_cases if (exp_dir / f"{name}.bin").exists()]
            if mp4_cases and act_dir.exists():
                act_bins = [act_dir / f"{name}.bin" for name in mp4_cases if (act_dir / f"{name}.bin").exists()]

            exp_count = len(exp_bins)
            act_count = len(act_bins)

            for name in mp4_cases:
                e = exp_dir / f"{name}.bin"
                a = act_dir / f"{name}.bin"
                if files_identical(e, a):
                    pass_count += 1

            if mp4_cases:
                covered = "yes" if pass_count == len(mp4_cases) else "no"

        out = dict(row)
        out["expected_bins"] = str(exp_count)
        out["actual_bins"] = str(act_count)
        out["pass_bins"] = str(pass_count)
        out["covered"] = covered
        out_rows.append(out)

    fieldnames = list(out_rows[0].keys())
    # Ensure our columns exist and are last-ish.
    for k in ["expected_bins", "actual_bins", "pass_bins", "covered"]:
        if k in fieldnames:
            fieldnames.remove(k)
            fieldnames.append(k)

    tmp = csv_path.with_suffix(".csv.tmp")
    with tmp.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fieldnames)
        w.writeheader()
        for row in out_rows:
            w.writerow(row)

    tmp.replace(csv_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
