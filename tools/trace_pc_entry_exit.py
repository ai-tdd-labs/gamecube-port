#!/usr/bin/env python3
"""
Trace RVZ execution by breaking at a function entry PC, then breaking again at
the function's return address (LR).

This is used to harvest "input/output snapshot" pairs from a running game in
Dolphin without modifying the game or printing logs.

Outputs are intentionally binary-first (RAM dumps + register JSON) so they can
be turned into deterministic unit tests later.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import time
import signal

# Reuse the Dolphin GDB client + headless launcher used by tools/ram_dump.py.
from ram_dump import DolphinGDB, start_dolphin  # type: ignore


# GDB PowerPC register numbers (hex):
# r0..r31: 0x00..0x1F
# pc/nip:  0x40
# msr:     0x41
# cr:      0x42
# lr:      0x43
# ctr:     0x44
# xer:     0x45
REG_PC = 0x40
REG_MSR = 0x41
REG_CR = 0x42
REG_LR = 0x43
REG_CTR = 0x44
REG_XER = 0x45
REG_R1 = 0x01
REG_ARG_FIRST = 0x03  # r3
REG_ARG_LAST = 0x0A   # r10


def parse_int(s: str) -> int:
    return int(s, 0)


def sha256_file(p: Path) -> str:
    h = hashlib.sha256()
    with p.open("rb") as f:
        while True:
            b = f.read(1024 * 1024)
            if not b:
                break
            h.update(b)
    return h.hexdigest()


def sha256_bytes(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()


def gdb_read_reg_u32(gdb: DolphinGDB, regno: int) -> int | None:
    # GDB remote "p" reads a single register.
    gdb._send(f"p{regno:x}")  # pylint: disable=protected-access
    payload = gdb._recv_packet_payload(timeout=2)  # pylint: disable=protected-access
    if payload is None:
        return None
    payload = payload.strip()
    if payload.startswith(b"E") or not payload:
        return None
    try:
        # Dolphin returns register as hex bytes, endianness already normalized by the stub.
        return int(payload.decode("ascii", errors="ignore"), 16) & 0xFFFFFFFF
    except Exception:
        return None


def write_json(p: Path, obj: dict) -> None:
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(obj, indent=2, sort_keys=True) + "\n")


def dump_range(gdb: DolphinGDB, addr: int, size: int, out_path: Path, chunk: int) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)
    tmp = out_path.with_suffix(out_path.suffix + ".tmp")
    with tmp.open("wb") as f:
        ok = gdb.read_memory_to_file(addr, size, f, chunk_size=chunk)  # type: ignore[arg-type]
    if not ok:
        raise RuntimeError(f"failed to dump 0x{addr:08X}+0x{size:X} to {out_path}")
    os.replace(tmp, out_path)


def parse_dump_spec(s: str) -> tuple[str, str, int]:
    """
    Parse --dump name:addr:size.

    addr supports:
    - absolute address: 0x80300000
    - register-relative: @r3, @r3+0x20, @r1-0x10
    """
    parts = s.split(":")
    if len(parts) != 3:
        raise argparse.ArgumentTypeError("dump spec must be name:addr:size")
    name, addr_s, size_s = parts
    if not name or any(c not in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_" for c in name):
        raise argparse.ArgumentTypeError("dump name must be [A-Za-z0-9_]+")
    size = parse_int(size_s)
    if size <= 0:
        raise argparse.ArgumentTypeError("dump size must be >0")
    # addr is validated when resolved (supports @reg expressions).
    return name, addr_s, size


def resolve_dump_addr(expr: str, regs_entry: dict[str, int]) -> int | None:
    """
    Resolve dump addr expression using entry registers.
    This lets us dump e.g. @r3 (pointer argument) deterministically even if the
    function is called with stack buffers.
    """
    s = expr.strip()
    if not s:
        return None
    if not s.startswith("@"):
        try:
            v = parse_int(s)
            return v if v >= 0 else None
        except Exception:
            return None

    # @r3, @r3+0x20, @r1-0x10
    s = s[1:]
    op = "+"
    base = s
    off_s = "0"
    if "+" in s:
        base, off_s = s.split("+", 1)
        op = "+"
    elif "-" in s:
        base, off_s = s.split("-", 1)
        op = "-"
    base = base.strip().lower()
    off_s = off_s.strip()
    if not base:
        return None
    if base not in regs_entry:
        return None
    try:
        off = parse_int(off_s)
    except Exception:
        return None
    v = regs_entry[base]
    if op == "+":
        addr = (v + off) & 0xFFFFFFFF
    else:
        addr = (v - off) & 0xFFFFFFFF
    return addr


def main() -> None:
    ap = argparse.ArgumentParser(description="Trace function entry/exit snapshots in Dolphin RVZ")
    ap.add_argument("--rvz", required=True, help="Path to RVZ/ISO to run in Dolphin")
    ap.add_argument("--entry-pc", type=parse_int, required=True, help="Function entry PC address (hex ok)")
    ap.add_argument("--out-dir", default="tests/trace-harvest/os_disable_interrupts/mp4_rvz", help="Output directory")
    ap.add_argument("--max-unique", type=int, default=25, help="Stop after collecting N unique input snapshots")
    ap.add_argument("--max-hits", type=int, default=2000, help="Hard cap on total breakpoint hits processed")
    ap.add_argument("--delay", type=float, default=8.0, help="Seconds to wait after starting Dolphin")
    ap.add_argument("--movie", default=None, help="Optional Dolphin DTM movie file (passed to Dolphin -m)")
    ap.add_argument("--timeout", type=float, default=60.0, help="Overall time limit in seconds")
    ap.add_argument("--gdb-host", default="127.0.0.1", help="Dolphin GDB host")
    ap.add_argument("--gdb-port", type=int, default=9090, help="Dolphin GDB port")
    ap.add_argument(
        "--dump",
        type=parse_dump_spec,
        action="append",
        default=[],
        help="Dump window: name:addr:size (repeatable). If omitted, dumps sdk_state only.",
    )
    ap.add_argument("--sdk-state-addr", type=parse_int, default=0x817FE000, help="(deprecated) sdk_state base address")
    ap.add_argument("--sdk-state-size", type=parse_int, default=0x2000, help="(deprecated) sdk_state size")
    ap.add_argument("--chunk", type=parse_int, default=0x1000, help="RAM dump chunk size")
    ap.add_argument("--enable-mmu", action="store_true", help="Enable Dolphin MMU via -C override")
    args = ap.parse_args()

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    trace_index = out_dir / "trace.jsonl"

    config_overrides: list[str] = []
    if args.enable_mmu:
        config_overrides.append("Core.MMU=True")

    dolphin_proc = start_dolphin(args.rvz, config_overrides=config_overrides, movie_path=args.movie)
    if dolphin_proc is None:
        raise SystemExit(1)
    time.sleep(args.delay)

    gdb = DolphinGDB(args.gdb_host, args.gdb_port)
    # Dolphin can take a bit to boot and open the stub; retry briefly.
    if not gdb.connect():
        if not gdb.reset_connection(retries=20, delay=0.25):
            dolphin_proc.terminate()
            raise SystemExit(1)
    if gdb.sock is None:
        dolphin_proc.terminate()
        raise SystemExit(1)

    entry_pc = int(args.entry_pc)
    dumps: list[tuple[str, str, int]] = list(args.dump)
    if not dumps:
        # Backwards-compatible default: dump sdk_state only.
        dumps = [("sdk_state", f"0x{int(args.sdk_state_addr):X}", int(args.sdk_state_size))]

    # We'll use real breakpoints (Z0/Z1). If that fails, the right fallback is
    # ram_dump.py's PC polling mode, but for trace harvesting we require real BPs.
    if not gdb.set_sw_breakpoint(entry_pc):
        dolphin_proc.terminate()
        raise SystemExit(f"failed to set entry breakpoint @ 0x{entry_pc:08X}")

    seen_inputs: set[str] = set()
    unique = 0
    hits = 0
    start = time.time()

    try:
        gdb.cont()

        while unique < args.max_unique and hits < args.max_hits and (time.time() - start) < args.timeout:
            stop = gdb.wait_stop(timeout=2.0)
            if stop is None:
                continue

            pc = gdb_read_reg_u32(gdb, REG_PC)
            if pc is None:
                # Try to keep going; Dolphin sometimes emits console packets.
                gdb.cont()
                continue

            if pc != entry_pc:
                # Not our stop; continue.
                gdb.cont()
                continue

            hits += 1

            lr = gdb_read_reg_u32(gdb, REG_LR)
            msr = gdb_read_reg_u32(gdb, REG_MSR)
            cr = gdb_read_reg_u32(gdb, REG_CR)
            ctr = gdb_read_reg_u32(gdb, REG_CTR)
            xer = gdb_read_reg_u32(gdb, REG_XER)
            r1 = gdb_read_reg_u32(gdb, REG_R1)
            args_in: dict[str, str] = {}
            regs_entry: dict[str, int] = {}
            for regno in range(REG_ARG_FIRST, REG_ARG_LAST + 1):
                v = gdb_read_reg_u32(gdb, regno)
                if v is not None:
                    args_in[f"r{regno}"] = f"0x{v:08X}"
                    regs_entry[f"r{regno}"] = v
            if r1 is not None:
                regs_entry["r1"] = r1

            if lr is None:
                gdb.cont()
                continue

            case_id = f"hit_{hits:06d}_pc_{pc:08X}_lr_{lr:08X}"
            case_dir = out_dir / case_id
            case_dir.mkdir(parents=True, exist_ok=True)

            in_regs = case_dir / "in_regs.json"
            out_regs = case_dir / "out_regs.json"

            in_bins: dict[str, Path] = {}
            out_bins: dict[str, Path] = {}
            resolved_dumps: list[tuple[str, int, int]] = []
            for name, addr_expr, size in dumps:
                addr = resolve_dump_addr(addr_expr, regs_entry)
                if addr is None:
                    raise RuntimeError(
                        f"failed to resolve dump addr '{addr_expr}' (have regs: {sorted(regs_entry.keys())})"
                    )
                in_p = case_dir / f"in_{name}.bin"
                out_p = case_dir / f"out_{name}.bin"
                dump_range(gdb, addr, size, in_p, args.chunk)
                in_bins[name] = in_p
                out_bins[name] = out_p
                resolved_dumps.append((name, addr, size))
            write_json(
                in_regs,
                {
                    "pc": f"0x{pc:08X}",
                    "lr": f"0x{lr:08X}",
                    "msr": None if msr is None else f"0x{msr:08X}",
                    "cr": None if cr is None else f"0x{cr:08X}",
                    "ctr": None if ctr is None else f"0x{ctr:08X}",
                    "xer": None if xer is None else f"0x{xer:08X}",
                    "r1": None if r1 is None else f"0x{r1:08X}",
                    "args": args_in,
                },
            )

            # Dedupe by (all dump bytes + key regs/args).
            # This keeps disk use bounded when the game loops and repeats identical inputs.
            in_shas = {name: sha256_file(p) for name, p in in_bins.items()}
            dedupe_key = (
                sha256_bytes(json.dumps(in_shas, sort_keys=True).encode())
                + "|"
                + sha256_bytes(
                    json.dumps(
                        {"pc": pc, "lr": lr, "msr": msr, "cr": cr, "ctr": ctr, "xer": xer, "args": args_in},
                        sort_keys=True,
                    ).encode()
                )
            )
            if dedupe_key in seen_inputs:
                # Don't expand disk usage for repeated identical inputs.
                # Keep running.
                gdb.cont()
                continue

            seen_inputs.add(dedupe_key)

            # Set a temporary breakpoint at the return address and run until it hits.
            if not gdb.set_sw_breakpoint(lr):
                gdb.cont()
                continue

            gdb.cont()
            stop2 = gdb.wait_stop(timeout=5.0)
            pc2 = gdb_read_reg_u32(gdb, REG_PC)

            # Clean up LR breakpoint regardless.
            gdb.clear_sw_breakpoint(lr)

            if stop2 is None or pc2 != lr:
                # Didn't reach return site deterministically; skip this case.
                gdb.cont()
                continue

            # Capture output snapshot at return site.
            for name, addr, size in resolved_dumps:
                dump_range(gdb, addr, size, out_bins[name], args.chunk)
            msr2 = gdb_read_reg_u32(gdb, REG_MSR)
            cr2 = gdb_read_reg_u32(gdb, REG_CR)
            ctr2 = gdb_read_reg_u32(gdb, REG_CTR)
            xer2 = gdb_read_reg_u32(gdb, REG_XER)
            r12 = gdb_read_reg_u32(gdb, REG_R1)
            args_out: dict[str, str] = {}
            for regno in range(REG_ARG_FIRST, REG_ARG_LAST + 1):
                v = gdb_read_reg_u32(gdb, regno)
                if v is not None:
                    args_out[f"r{regno}"] = f"0x{v:08X}"
            write_json(
                out_regs,
                {
                    "pc": f"0x{pc2:08X}" if pc2 is not None else None,
                    "lr": f"0x{lr:08X}",
                    "msr": None if msr2 is None else f"0x{msr2:08X}",
                    "cr": None if cr2 is None else f"0x{cr2:08X}",
                    "ctr": None if ctr2 is None else f"0x{ctr2:08X}",
                    "xer": None if xer2 is None else f"0x{xer2:08X}",
                    "r1": None if r12 is None else f"0x{r12:08X}",
                    "args": args_out,
                },
            )

            unique += 1

            event = {
                "case_id": case_id,
                "entry_pc": f"0x{entry_pc:08X}",
                "pc": f"0x{pc:08X}",
                "lr": f"0x{lr:08X}",
                "in_sha256": in_shas,
                "out_sha256": {name: sha256_file(p) for name, p in out_bins.items()},
                "dedupe_key": dedupe_key,
            }
            with trace_index.open("a", encoding="utf-8") as f:
                f.write(json.dumps(event, sort_keys=True) + "\n")

            # Continue tracing.
            gdb.cont()

    finally:
        try:
            gdb.clear_sw_breakpoint(entry_pc)
        except Exception:
            pass
        try:
            gdb.close()
        finally:
            dolphin_proc.terminate()
            try:
                dolphin_proc.wait(timeout=5)
            except Exception:
                try:
                    dolphin_proc.kill()
                except Exception:
                    pass
                try:
                    dolphin_proc.wait(timeout=5)
                except Exception:
                    # Last-resort: don't hang trace harvesting on process teardown.
                    try:
                        os.kill(dolphin_proc.pid, signal.SIGKILL)
                    except Exception:
                        pass

    print(f"Collected unique={unique} (hits={hits}) into {out_dir}")


if __name__ == "__main__":
    main()
