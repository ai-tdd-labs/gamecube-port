#!/usr/bin/env python3
import os
import re
import sys
from dataclasses import dataclass
from fnmatch import fnmatch
from pathlib import Path
from typing import Optional

def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]

SDK_PREFIXES = ("OS", "GX", "VI", "PAD", "DVD", "SI", "AR", "AI", "CARD")

# Conservative identifier: prefix + [A-Za-z0-9_]*
SDK_CALL_RE = re.compile(r"\b(?:" + "|".join(SDK_PREFIXES) + r")[A-Za-z0-9_]*\s*\(")

# Better: capture name
SDK_CALL_CAP_RE = re.compile(r"\b((?:" + "|".join(SDK_PREFIXES) + r")[A-Za-z0-9_]*)\s*\(")


def to_snake(name: str) -> str:
    # OSDisableInterrupts -> os_disable_interrupts
    # OSRoundUp32B        -> os_round_up_32b
    s = name
    # Split initialisms: "OSRound" -> "OS_Round"
    s = re.sub(r"([A-Z]+)([A-Z][a-z])", r"\1_\2", s)
    # Split lower/digit before upper: "RoundUp" -> "Round_Up", "u8" unaffected here.
    s = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", s)
    # Split alpha<->digit boundaries: "Up32" -> "Up_32", "32B" -> "32_B"
    s = re.sub(r"([A-Za-z])([0-9])", r"\1_\2", s)
    s = re.sub(r"([0-9])([A-Za-z])", r"\1_\2", s)
    s = s.replace("__", "_").lower()
    # Collapse common SDK suffix patterns like "_32_b" -> "_32b".
    s = re.sub(r"_(\d+)_([a-z])\b", r"_\1\2", s)
    return s


def parse_workload_extra_srcs(run_host_scenario_sh: Path) -> list[Path]:
    txt = run_host_scenario_sh.read_text(encoding="utf-8", errors="replace").splitlines()
    in_workload = False
    in_extra = False
    out: list[Path] = []
    for line in txt:
        if re.match(r"\s*workload\)\s*$", line):
            in_workload = True
            continue
        if in_workload and re.match(r"\s*\w+\)\s*$", line) and not line.strip().startswith("workload"):
            # next case label
            if in_extra:
                in_extra = False
            # keep in_workload until we actually exit the case block; we just ignore further
        if in_workload and "extra_srcs+=(" in line:
            in_extra = True
            continue
        if in_extra:
            if line.strip().startswith(")"):
                break
            m = re.search(r'"\$repo_root/([^"]+)"', line)
            if m:
                out.append(Path(m.group(1)))
    return out


@dataclass(frozen=True)
class WorkloadCaseAdd:
    patterns: tuple[str, ...]
    relpaths: tuple[Path, ...]


def parse_workload_case_additions(run_host_scenario_sh: Path) -> list[WorkloadCaseAdd]:
    """
    Parse scenario-dependent extra_srcs additions from tools/run_host_scenario.sh.

    We don't try to fully interpret bash; we just extract the common pattern used in this repo:
      case "$scenario_base" in
        a|b|c)
          extra_srcs+=("$repo_root/path/to/file.c")
          ...
          ;;
      esac
    """
    txt = run_host_scenario_sh.read_text(encoding="utf-8", errors="replace").splitlines()

    in_workload = False
    in_case = False
    in_scenario_case = False

    cur_patterns: list[str] = []
    cur_paths: list[Path] = []
    out: list[WorkloadCaseAdd] = []

    def flush() -> None:
        nonlocal cur_patterns, cur_paths
        if cur_patterns and cur_paths:
            out.append(WorkloadCaseAdd(patterns=tuple(cur_patterns), relpaths=tuple(cur_paths)))
        cur_patterns = []
        cur_paths = []

    for line in txt:
        if re.match(r"\s*workload\)\s*$", line):
            in_workload = True
            continue
        if not in_workload:
            continue

        if re.match(r"^\s*case\s+\"\$scenario_base\"\s+in\s*$", line):
            in_case = True
            continue
        if in_case and re.match(r"^\s*esac\s*$", line):
            flush()
            in_case = False
            in_scenario_case = False
            continue

        if not in_case:
            continue

        # Pattern line: "mp4_a|mp4_b|mp4_process_*)"
        m = re.match(r"^\s*([a-zA-Z0-9_.*|]+)\)\s*$", line)
        if m:
            flush()
            cur_patterns = [p.strip() for p in m.group(1).split("|") if p.strip()]
            in_scenario_case = True
            continue

        if in_scenario_case:
            # Collect extra_srcs additions.
            m2 = re.search(r'extra_srcs\+\=\(\s*"\$repo_root/([^"]+)"\s*\)', line)
            if m2:
                cur_paths.append(Path(m2.group(1)))
                continue

            # Also handle the style: extra_srcs+=( "<line>" "<line>" )
            m3 = re.search(r'"\$repo_root/([^"]+)"', line)
            if "extra_srcs+=" in line and m3:
                cur_paths.append(Path(m3.group(1)))
                continue

            if re.match(r"^\s*;;\s*$", line):
                flush()
                in_scenario_case = False
                continue

    flush()
    return out


@dataclass(frozen=True)
class Evidence:
    unified_runner: Optional[Path]
    property_runner: Optional[Path]
    has_tests_dir: Optional[Path]
    has_trace_replay: bool


def cov_tags(func: str, ev: Evidence) -> list[str]:
    # Treat obvious SDK macros as "no test needed". This keeps the report honest
    # without requiring a suite directory per macro.
    if func in {"PADButtonDown"}:
        return ["macro_no_test_needed"]

    tags: list[str] = []
    if ev.unified_runner:
        tags.append("unified_dol_vs_host")
    if ev.has_trace_replay:
        tags.append("rvz_trace_replay")
    if ev.property_runner:
        tags.append("property")
    if ev.has_tests_dir and not ev.unified_runner:
        tags.append("dol_vs_host_or_legacy")
    if not tags:
        tags.append("workload_only_or_unknown")
    return tags


def find_evidence(rr: Path, func: str) -> Evidence:
    snake = to_snake(func)
    unified = rr / "tools" / f"run_{snake}_pbt.sh"
    prop = rr / "tools" / f"run_{snake}_property_test.sh"

    # tests dir heuristic: tests/sdk/<module>/<snake>/...
    mod = snake.split("_")[0]  # os/gx/vi/pad/dvd/si/ar/ai/card
    alias_dir = {
        # OSRestoreInterrupts is exercised by the OSDisableInterrupts suite.
        "OSRestoreInterrupts": rr / "tests" / "sdk" / "os" / "os_disable_interrupts",
    }.get(func)

    tests_dir = alias_dir if alias_dir is not None else (rr / "tests" / "sdk" / mod / snake)
    if not tests_dir.is_dir():
        # Some suites use a different directory name (e.g. GXClearVCacheMetric -> gx_clear_vcache_metric)
        alt = snake.replace("_vcache_", "_vcache_").replace("_v_cache_", "_vcache_").replace("_v_cache", "_vcache")
        alt2 = snake.replace("_vcache_", "_vcache_")  # keep for symmetry
        # Specific known mismatch: v_cache vs vcache.
        alt3 = snake.replace("_v_cache_", "_vcache_")
        for cand in {alt, alt2, alt3, snake.replace("_v_cache", "_vcache")}:
            p = rr / "tests" / "sdk" / mod / cand
            if p.is_dir():
                tests_dir = p
                break
    has_tests_dir = tests_dir if tests_dir.is_dir() else None

    has_trace = False
    if has_tests_dir:
        # RVZ trace replay scenarios live under host/*rvz_trace_replay*
        host_dir = has_tests_dir / "host"
        if host_dir.is_dir():
            for p in host_dir.glob("*rvz_trace_replay*"):
                has_trace = True
                break

    return Evidence(
        unified_runner=unified if unified.exists() else None,
        property_runner=prop if prop.exists() else None,
        has_tests_dir=has_tests_dir,
        has_trace_replay=has_trace,
    )


def scan_sdk_calls(path: Path) -> set[str]:
    try:
        txt = path.read_text(encoding="utf-8", errors="replace")
    except Exception:
        return set()

    # Strip C/C++ comments crudely to reduce false positives.
    txt = re.sub(r"/\*.*?\*/", " ", txt, flags=re.S)
    txt = re.sub(r"//.*", " ", txt)

    out: set[str] = set()
    for m in SDK_CALL_CAP_RE.finditer(txt):
        out.add(m.group(1))
    return out


def main() -> int:
    rr = repo_root()

    run_host = rr / "tools" / "run_host_scenario.sh"
    extra_rel = parse_workload_extra_srcs(run_host)
    extra_srcs = [rr / p for p in extra_rel]
    case_adds = parse_workload_case_additions(run_host)

    # Keep the scenario set aligned with tools/run_mp4_workload_ladder.sh (reachability ladder).
    scenarios = {
        "husysinit": rr / "tests" / "workload" / "mp4" / "mp4_husysinit_001_scenario.c",
        "huprcinit": rr / "tests" / "workload" / "mp4" / "mp4_huprcinit_001_scenario.c",
        "hupadinit": rr / "tests" / "workload" / "mp4" / "mp4_hupadinit_001_scenario.c",
        "gwinit": rr / "tests" / "workload" / "mp4" / "mp4_gwinit_001_scenario.c",
        "pfinit": rr / "tests" / "workload" / "mp4" / "mp4_pfinit_001_scenario.c",
        "husprinit": rr / "tests" / "workload" / "mp4" / "mp4_husprinit_001_scenario.c",
        "hu3dinit": rr / "tests" / "workload" / "mp4" / "mp4_hu3dinit_001_scenario.c",
        "hudatainit": rr / "tests" / "workload" / "mp4" / "mp4_hudatainit_001_scenario.c",
        "huperfinit": rr / "tests" / "workload" / "mp4" / "mp4_huperfinit_001_scenario.c",
        "init_to_viwait": rr / "tests" / "workload" / "mp4" / "mp4_init_to_viwait_001_scenario.c",
        "mainloop_one_iter": rr / "tests" / "workload" / "mp4" / "mp4_mainloop_one_iter_001_scenario.c",
        "mainloop_one_iter_tick": rr / "tests" / "workload" / "mp4" / "mp4_mainloop_one_iter_tick_001_scenario.c",
        "mainloop_one_iter_tick_pf_draw": rr / "tests" / "workload" / "mp4" / "mp4_mainloop_one_iter_tick_pf_draw_001_scenario.c",
        "mainloop_two_iter": rr / "tests" / "workload" / "mp4" / "mp4_mainloop_two_iter_001_scenario.c",
        "mainloop_two_iter_tick": rr / "tests" / "workload" / "mp4" / "mp4_mainloop_two_iter_tick_001_scenario.c",
        "mainloop_ten_iter_tick": rr / "tests" / "workload" / "mp4" / "mp4_mainloop_ten_iter_tick_001_scenario.c",
        "mainloop_hundred_iter_tick": rr / "tests" / "workload" / "mp4" / "mp4_mainloop_hundred_iter_tick_001_scenario.c",
        "mainloop_thousand_iter_tick": rr / "tests" / "workload" / "mp4" / "mp4_mainloop_thousand_iter_tick_001_scenario.c",
        "wipe_frame_still_mtx": rr / "tests" / "workload" / "mp4" / "mp4_wipe_frame_still_mtx_001_scenario.c",
        "wipe_crossfade_mtx": rr / "tests" / "workload" / "mp4" / "mp4_wipe_crossfade_mtx_001_scenario.c",
        "process_scheduler": rr / "tests" / "workload" / "mp4" / "mp4_process_scheduler_001_scenario.c",
        "process_sleep": rr / "tests" / "workload" / "mp4" / "mp4_process_sleep_001_scenario.c",
        "process_vsleep": rr / "tests" / "workload" / "mp4" / "mp4_process_vsleep_001_scenario.c",
    }

    def scenario_extra_srcs(scenario_path: Path) -> list[Path]:
        scen_base = scenario_path.stem
        out: list[Path] = []
        for add in case_adds:
            if any(fnmatch(scen_base, pat) for pat in add.patterns):
                out.extend(rr / p for p in add.relpaths)
        return out

    # Collect calls per scenario based on the exact source set compiled by the runner.
    per_scenario: dict[str, set[str]] = {}
    for label, scen in scenarios.items():
        files = [scen] + extra_srcs + scenario_extra_srcs(scen)
        calls: set[str] = set()
        for f in files:
            if f.suffix not in (".c", ".S"):
                continue
            if not f.exists():
                continue
            calls |= scan_sdk_calls(f)
        per_scenario[label] = calls

    all_calls = set().union(*per_scenario.values())

    # Build evidence map.
    evid = {c: find_evidence(rr, c) for c in sorted(all_calls)}

    # Emit markdown.
    print("# MP4 Workload SDK Coverage Audit")
    print()
    print("Generated by: `python3 tools/helpers/mp4_workload_sdk_coverage_report.py`")
    print()
    print("This is a static source scan of the MP4 host workload build inputs (as wired by `tools/run_host_scenario.sh` under `workload)`).")
    print("It answers: which SDK functions are referenced by the compiled workload sources, and what deterministic evidence exists in-repo for each.")
    print()

    for label in scenarios.keys():
        calls = sorted(per_scenario[label])
        print(f"## Scenario: {label}")
        print()
        print(f"Source entry: `{scenarios[label].relative_to(rr)}`")
        print()
        print("| SDK function | Evidence types | Evidence paths |")
        print("|---|---|---|")
        for c in calls:
            ev = evid[c]
            tags = ", ".join(cov_tags(c, ev))
            paths: list[str] = []
            if ev.unified_runner:
                paths.append(str(ev.unified_runner.relative_to(rr)))
            if ev.property_runner:
                paths.append(str(ev.property_runner.relative_to(rr)))
            if ev.has_tests_dir:
                paths.append(str(ev.has_tests_dir.relative_to(rr)) + "/")
            if not paths:
                paths.append("-")
            print(f"| `{c}` | `{tags}` | " + "<br/>".join(f"`{p}`" for p in paths) + " |")
        print()

    # Summary: workload-referenced but no deterministic oracle evidence.
    missing = []
    for c in sorted(all_calls):
        ev = evid[c]
        tags = cov_tags(c, ev)
        if "macro_no_test_needed" in tags:
            continue
        if ev.unified_runner or ev.has_tests_dir or ev.property_runner:
            continue
        missing.append(c)

    print("## Summary: Workload-referenced functions without per-function evidence")
    print()
    if not missing:
        print("All workload-referenced SDK functions have some tests/runner evidence in `tests/sdk` or `tools/`.")
    else:
        print("These functions are referenced by the workload build inputs, but I did not find a matching `tools/run_<func>_pbt.sh`, `tools/run_<func>_property_test.sh`, or `tests/sdk/<module>/<func>/` directory:")
        print()
        for c in missing:
            print(f"- `{c}`")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
