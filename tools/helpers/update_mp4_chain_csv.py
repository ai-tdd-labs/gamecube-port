#!/usr/bin/env python3
import csv
from pathlib import Path
import subprocess
import sys
from typing import Optional


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
    # Usage:
    #   tools/helpers/update_mp4_chain_csv.py
    #   tools/helpers/update_mp4_chain_csv.py docs/sdk/mp4/MP4_chain_all.csv
    #
    # This helper counts only MP4 callsite testcases (dol/mp4/*) for each canonical suite.
    # The canonical MP4 chain tracker is a single combined CSV (HuSysInit + main-loop + planned batches).
    default_csv = root / "docs/sdk/mp4/MP4_chain_all.csv"
    csv_path = default_csv
    if len(sys.argv) >= 2:
        csv_path = Path(sys.argv[1])
        if not csv_path.is_absolute():
            csv_path = root / csv_path
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
        tests_exp_act = (row.get("tests_exp_act") or "").strip()

        if suite:
            suite_path = root / suite
            exp_dir = suite_path / "expected"
            act_dir = suite_path / "actual"
            suite_slug = suite_path.name

            # Only count MP4 callsite testcases (dol/mp4/*), not generic or other-game variants.
            #
            # IMPORTANT: we must NOT depend on built artifacts (.dol files), because they are usually
            # gitignored. The canonical case id is the directory name under dol/mp4/.
            mp4_dol_root = suite_path / "dol" / "mp4"
            mp4_cases: list[str] = []
            if mp4_dol_root.exists():
                for p in sorted(mp4_dol_root.iterdir()):
                    if not p.is_dir():
                        continue

                    # Canonical case id should match the produced .dol/.bin stem.
                    # Prefer reading built artifact names (if present), otherwise fall back
                    # to Makefile TARGET, otherwise directory name.
                    dols = sorted(p.glob("*.dol"))
                    if dols:
                        mp4_cases.extend([d.stem for d in dols])
                        continue

                    mk = p / "Makefile"
                    if mk.exists():
                        for line in mk.read_text().splitlines():
                            line = line.strip()
                            if line.startswith("TARGET :="):
                                mp4_cases.append(line.split(":=", 1)[1].strip())
                                break
                        else:
                            mp4_cases.append(p.name)
                    else:
                        mp4_cases.append(p.name)

            def resolve_bin(dirpath: Path, case: str) -> Optional[Path]:
                # Convention: expected/actual bins are usually prefixed with the suite slug.
                # Example: suite_slug=os_get_arena_lo, case=realistic_mem_alloc_001
                # -> os_get_arena_lo_realistic_mem_alloc_001.bin
                a = dirpath / f"{case}.bin"
                if a.exists():
                    return a
                b = dirpath / f"{suite_slug}_{case}.bin"
                if b.exists():
                    return b
                c = dirpath / f"{suite_slug}_mp4_{case}.bin"
                if c.exists():
                    return c
                return None

            exp_bins = []
            act_bins = []
            if mp4_cases and exp_dir.exists():
                for name in mp4_cases:
                    p = resolve_bin(exp_dir, name)
                    if p is not None:
                        exp_bins.append(p)
            if mp4_cases and act_dir.exists():
                for name in mp4_cases:
                    p = resolve_bin(act_dir, name)
                    if p is not None:
                        act_bins.append(p)

            exp_count = len(exp_bins)
            act_count = len(act_bins)

            for name in mp4_cases:
                e = resolve_bin(exp_dir, name) if exp_dir.exists() else None
                a = resolve_bin(act_dir, name) if act_dir.exists() else None
                if e is None or a is None:
                    continue
                if files_identical(e, a):
                    pass_count += 1

            if mp4_cases:
                # Keep legacy/manual column in sync: "pass/total" (MP4-only cases).
                tests_exp_act = f"{pass_count}/{len(mp4_cases)}"
                covered = "y" if pass_count == len(mp4_cases) and pass_count > 0 else ""

        out = dict(row)
        out["tests_exp_act"] = tests_exp_act if tests_exp_act else ("0/0" if suite else "")
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
