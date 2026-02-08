#!/usr/bin/env python3
"""
RAM Dump Tool for Dolphin Emulator

Connects to Dolphin's GDB stub and dumps RAM to a file.
Used for creating expected.bin files from Dolphin for unit tests.

Usage:
    # With Dolphin already running (GDB stub enabled):
    python3 ram_dump.py --addr 0x80300000 --size 4096 --out expected.bin

    # Auto-start Dolphin with a DOL/ISO:
    python3 ram_dump.py --exec test.dol --addr 0x80300000 --size 4096 --out expected.bin
    python3 ram_dump.py --exec game.iso --addr 0x80300000 --size 4096 --out expected.bin

    # With delay to let game initialize:
    python3 ram_dump.py --exec test.dol --delay 2 --addr 0x80300000 --size 4096 --out expected.bin
"""

import socket
import argparse
import sys
import time
import subprocess
import os
import signal
import tempfile


class DolphinGDB:
    """Simple GDB client for Dolphin's GDB stub."""

    def __init__(self, host='127.0.0.1', port=9090):
        self.host = host
        self.port = port
        self.sock = None
        self._rxbuf = b""

    def connect(self):
        """Connect to Dolphin GDB stub."""
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(10)
        try:
            self.sock.connect((self.host, self.port))
            print(f"Connected to Dolphin at {self.host}:{self.port}")
            return True
        except Exception as e:
            print(f"Failed to connect: {e}")
            print("Make sure Dolphin is running with GDB stub enabled.")
            return False

    def reset_connection(self, retries=5, delay=0.2):
        """Reconnect to the GDB stub.

        Large/failed memory reads can leave the TCP stream in a bad state.
        Reconnecting is cheap and makes large dumps far more reliable.
        """
        try:
            if self.sock:
                self.sock.close()
        except Exception:
            pass
        self.sock = None
        self._rxbuf = b""
        for i in range(retries):
            if self.connect():
                return True
            time.sleep(delay)
        return False

    def _send(self, cmd):
        """Send GDB command with checksum."""
        if not self.sock:
            raise RuntimeError("GDB socket is not connected")
        checksum = sum(cmd.encode()) % 256
        packet = f"${cmd}#{checksum:02x}"
        self.sock.send(packet.encode())

    def _recv_packet_payload(self, timeout=5):
        """Receive exactly one GDB remote protocol packet payload.

        IMPORTANT: The TCP stream may contain multiple packets back-to-back.
        We must not conflate them, or large dumps can become misaligned.
        """
        self.sock.settimeout(timeout)
        start = time.time()

        while time.time() - start < timeout:
            # Try to parse a packet from the buffered data.
            dollar = self._rxbuf.find(b"$")
            if dollar >= 0:
                # Drop leading noise like '+' ACKs.
                if dollar > 0:
                    self._rxbuf = self._rxbuf[dollar:]
                    dollar = 0

                hash_idx = self._rxbuf.find(b"#", dollar + 1)
                if hash_idx >= 0 and len(self._rxbuf) >= hash_idx + 3:
                    pkt = self._rxbuf[:hash_idx + 3]
                    self._rxbuf = self._rxbuf[hash_idx + 3:]

                    # ACK the packet.
                    try:
                        self.sock.send(b"+")
                    except Exception:
                        pass

                    payload = pkt[1:hash_idx]
                    return payload

            # Need more bytes.
            try:
                chunk = self.sock.recv(4096)
                if not chunk:
                    break
                self._rxbuf += chunk
            except socket.timeout:
                continue

        return None

    def read_memory(self, addr, size):
        """Read memory from Dolphin. Returns hex string or None."""
        self._send(f'm{addr:x},{size:x}')
        # Dolphin may interleave async packets (console output 'O', stop reply
        # 'S'/'T'). Drain until we get the memory response.
        for _ in range(256):
            payload = self._recv_packet_payload()
            if payload is None:
                return None

            payload = payload.strip()
            if not payload:
                continue

            # Async console output (hex-encoded) - ignore.
            if payload.startswith(b"O"):
                continue

            # Stop replies - ignore (can occur after Ctrl-C).
            if payload.startswith(b"S") or payload.startswith(b"T"):
                continue

            # Other benign replies.
            if payload == b"OK":
                continue

            hex_data = payload.decode(errors="replace").strip()
            # GDB remote protocol error replies are "E" + 2 hex digits (length 3).
            # Memory data itself is hex bytes and may validly start with 'E' (e.g. 0xEC...).
            if len(hex_data) == 3 and hex_data[0] == "E":
                return None
            for ch in hex_data:
                if ch not in "0123456789abcdefABCDEF":
                    return None
            return hex_data

        return None
        return None

    def read_memory_bytes(self, addr, size, chunk_size=0x10000, chunk_retries=3):
        """Read memory and return as bytes.

        We prefer larger chunk sizes for big dumps (e.g. MEM1) to reduce GDB
        stub overhead, and we retry per-chunk because Dolphin startup can be
        transiently flaky.
        """
        data = bytearray()
        remaining = size
        cursor = addr

        while remaining > 0:
            chunk = min(remaining, chunk_size)
            hex_data = None
            for attempt in range(chunk_retries):
                hex_data = self.read_memory(cursor, chunk)
                if hex_data:
                    break
                time.sleep(0.05)

            if not hex_data:
                # Try smaller chunks.
                if chunk_size > 0x200:
                    chunk_size = chunk_size // 2
                    continue
                print(f"Failed to read at 0x{cursor:08X}")
                return None

            b = bytes.fromhex(hex_data)
            if len(b) != chunk:
                # If the stub returns a malformed size, do not accept it (it would
                # desync the whole dump). Retry or fall back to smaller chunks.
                if chunk_size > 0x200:
                    chunk_size = chunk_size // 2
                    continue
                print(f"Malformed read at 0x{cursor:08X}: got {len(b)} bytes, expected {chunk}")
                return None

            data.extend(b)
            remaining -= chunk
            cursor += chunk

            # Progress indicator
            pct = ((size - remaining) / size) * 100
            print(f"\rReading: {pct:.1f}% (0x{cursor:08X})", end="", flush=True)

        print()  # newline after progress
        return bytes(data)

    def read_memory_to_file(self, addr, size, out_f, chunk_size=0x10000, chunk_retries=3):
        """Stream a memory dump directly to a file.

        This is safer for large dumps (e.g. MEM1 24 MiB) because we don't keep
        the whole dump in memory.
        """
        remaining = size
        cursor = addr
        initial_chunk = chunk_size

        while remaining > 0:
            chunk = min(remaining, chunk_size)
            hex_data = None
            for attempt in range(chunk_retries):
                hex_data = self.read_memory(cursor, chunk)
                if hex_data:
                    break
                time.sleep(0.05)

            if not hex_data:
                # A failed read can desync the TCP stream. Reconnect before we
                # start shrinking chunks, otherwise subsequent reads may fail
                # even at small sizes.
                if not self.reset_connection():
                    print("Failed to reconnect to Dolphin GDB stub.")
                    return False
                if chunk_size > 0x200:
                    chunk_size = chunk_size // 2
                    continue
                print(f"Failed to read at 0x{cursor:08X}")
                return False

            try:
                b = bytes.fromhex(hex_data)
            except Exception:
                b = b""

            if len(b) != chunk:
                if not self.reset_connection():
                    print("Failed to reconnect to Dolphin GDB stub.")
                    return False
                if chunk_size > 0x200:
                    chunk_size = chunk_size // 2
                    continue
                print(f"Malformed read at 0x{cursor:08X}: got {len(b)} bytes, expected {chunk}")
                return False

            out_f.write(b)
            remaining -= chunk
            cursor += chunk

            pct = ((size - remaining) / size) * 100
            print(f"\rReading: {pct:.1f}% (0x{cursor:08X}, chunk=0x{chunk_size:X})", end="", flush=True)

            # If we had to fall back to smaller chunks, opportunistically try
            # to increase again (bounded by the initial value).
            if chunk_size < initial_chunk and remaining > 0 and (cursor & 0xFFFFF) == 0:
                chunk_size = min(initial_chunk, chunk_size * 2)

        print()
        return True

    def halt(self):
        """Halt CPU execution.

        Returns the first stop-reply packet payload (b"S.." or b"T..") if seen.
        """
        self.sock.send(b'\x03')
        # Dolphin replies with a stop packet (e.g. "Sxx" / "Txx"), but it can
        # interleave async console output packets ('O'). Drain until stop.
        stop = None
        for _ in range(64):
            payload = self._recv_packet_payload(timeout=2)
            if payload is None:
                break
            payload = payload.strip()
            if payload.startswith(b"S") or payload.startswith(b"T"):
                stop = payload
                break
        time.sleep(0.1)
        return stop

    def cont(self):
        """Continue CPU execution."""
        self._send('c')

    def set_sw_breakpoint(self, addr, kind=4):
        """Set a breakpoint via GDB remote protocol.

        Dolphin's stub doesn't always implement Z0 (software breakpoints).
        We'll try Z0 first, then Z1 (hardware breakpoint) as a fallback.
        """
        for z in ("Z0", "Z1"):
            self._send(f"{z},{addr:x},{kind:x}")
            for _ in range(64):
                payload = self._recv_packet_payload(timeout=2)
                if payload is None:
                    break
                payload = payload.strip()
                if payload == b"OK":
                    return True
                # Async console output (hex-encoded) - ignore.
                # NOTE: must check OK first; it also starts with 'O'.
                if payload.startswith(b"O"):
                    continue
                if payload.startswith(b"E"):
                    break
        return False

    def clear_sw_breakpoint(self, addr, kind=4):
        """Clear a breakpoint via GDB remote protocol (z0/z1)."""
        ok = False
        for z in ("z0", "z1"):
            self._send(f"{z},{addr:x},{kind:x}")
            for _ in range(64):
                payload = self._recv_packet_payload(timeout=2)
                if payload is None:
                    break
                payload = payload.strip()
                if payload == b"OK":
                    ok = True
                    break
                # Async console output (hex-encoded) - ignore.
                # NOTE: must check OK first; it also starts with 'O'.
                if payload.startswith(b"O"):
                    continue
                if payload.startswith(b"E"):
                    break
        return ok

    def wait_stop(self, timeout=10.0):
        """Wait until the target stops (breakpoint/signal). Returns stop packet payload or None."""
        end = time.time() + timeout
        while time.time() < end:
            payload = self._recv_packet_payload(timeout=1)
            if payload is None:
                continue
            payload = payload.strip()
            if payload.startswith(b"O"):
                continue
            if payload.startswith(b"S") or payload.startswith(b"T"):
                return payload
        return None

    def run_and_halt(self, duration=0.5):
        """Continue execution for a duration then halt."""
        self.cont()
        time.sleep(duration)
        return self.halt()

    def close(self):
        if self.sock:
            self.sock.close()


def parse_stop_pc(stop_payload):
    """Extract PC from a Dolphin stop-reply packet.

    Example: b'T0540:800ba2f0;01:8019d798;'
    In Dolphin's GDB stub, reg 0x40 is typically NIP/PC.
    """
    if not stop_payload:
        return None
    try:
        s = stop_payload.decode("ascii", errors="ignore")
    except Exception:
        return None

    # Prefer reg 0x40 (PC/NIP).
    for key in ("40", "41"):
        needle = f"{key}:"
        idx = s.find(needle)
        if idx >= 0:
            j = idx + len(needle)
            k = s.find(";", j)
            if k < 0:
                k = len(s)
            val = s[j:k]
            try:
                return int(val, 16)
            except Exception:
                pass

    return None


def parse_int(s):
    """Parse int from string (supports 0x prefix)."""
    return int(s, 0)


# Default Dolphin path on macOS
DOLPHIN_BIN = "/Applications/Dolphin.app/Contents/MacOS/Dolphin"


def start_dolphin(exec_path, dolphin_bin=DOLPHIN_BIN, user_dir=None, config_overrides=None):
    """Start Dolphin in headless mode with GDB stub enabled."""
    if not os.path.isfile(dolphin_bin):
        print(f"Dolphin not found at {dolphin_bin}")
        return None

    if not os.path.isfile(exec_path):
        print(f"File not found: {exec_path}")
        return None

    # Use --exec flow so Dolphin starts headless with GDB stub active.
    # -b = batch (headless), -d = debugger (required for stub), -e = execute
    cmd = [dolphin_bin, "-b", "-d"]
    if user_dir:
        cmd += ["-u", user_dir]
    if config_overrides:
        for ov in config_overrides:
            cmd += ["-C", ov]
    cmd += ["-e", exec_path]
    print(f"Starting Dolphin: {' '.join(cmd)}")

    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL
    )
    return proc


def main():
    parser = argparse.ArgumentParser(
        description='Dump RAM from Dolphin emulator',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    # Dump 4KB at 0x80300000
    python3 ram_dump.py --addr 0x80300000 --size 4096 --out expected.bin

    # Dump with hex size
    python3 ram_dump.py --addr 0x80300000 --size 0x1000 --out expected.bin

    # Halt CPU before dump (for stable state)
    python3 ram_dump.py --addr 0x80300000 --size 4096 --out expected.bin --halt
        """
    )

    parser.add_argument('--addr', type=parse_int, required=True,
                        help='Start address (hex ok, e.g. 0x80300000)')
    parser.add_argument('--size', type=parse_int, required=True,
                        help='Size in bytes (hex ok, e.g. 0x1000)')
    parser.add_argument('--out', '-o', required=True,
                        help='Output file path')
    parser.add_argument('--exec', '-e', dest='exec_file',
                        help='DOL/ISO to run in Dolphin (auto-starts Dolphin)')
    parser.add_argument('--dolphin', default=DOLPHIN_BIN,
                        help=f'Path to Dolphin binary (default: {DOLPHIN_BIN})')
    parser.add_argument('--dolphin-userdir', default=None,
                        help='Optional Dolphin user dir (passed as -u). Use to keep per-test configs isolated.')
    parser.add_argument('--dolphin-config', action='append', default=[],
                        help='Optional Dolphin config override (repeatable). Format: System.Section.Key=Value (passed as -C).')
    parser.add_argument('--enable-mmu', action='store_true',
                        help='Convenience: attempt to enable MMU via Dolphin -C overrides (best-effort).')
    parser.add_argument('--delay', type=float, default=2.0,
                        help='Seconds to wait after starting Dolphin (default: 2)')
    parser.add_argument('--host', default='127.0.0.1',
                        help='Dolphin GDB host (default: 127.0.0.1)')
    parser.add_argument('--port', type=int, default=9090,
                        help='Dolphin GDB port (default: 9090)')
    parser.add_argument('--halt', action='store_true',
                        help='Halt CPU before reading (for stable state)')
    parser.add_argument('--run', type=float, default=0,
                        help='Run emulation for N seconds before halting (use with DOLs that write test data)')
    parser.add_argument('--continue', dest='cont', action='store_true',
                        help='Continue CPU after reading')

    # Dolphin's GDB stub appears to return incorrect data for larger reads that
    # span multiple pages. 4 KiB is slower but reliable for big MEM1 dumps.
    parser.add_argument('--chunk', type=parse_int, default=0x1000,
                        help='Read chunk size (default: 0x1000). Increase if stable on your setup.')
    parser.add_argument('--breakpoint', type=parse_int, default=None,
                        help='Optional software breakpoint address (e.g. 0x800057C0). If set, continue until hit, then dump.')
    parser.add_argument('--bp-timeout', type=float, default=15.0,
                        help='Seconds to wait for breakpoint hit (default: 15).')
    parser.add_argument('--pc-breakpoint', type=parse_int, default=None,
                        help='Optional PC/NIP checkpoint address. If set, poll-run/halts until PC==addr, then dump (works even when Z0/Z1 are unsupported).')
    parser.add_argument('--pc-timeout', type=float, default=15.0,
                        help='Seconds to wait for PC checkpoint (default: 15).')
    parser.add_argument('--pc-step', type=float, default=0.02,
                        help='Seconds to run between halt polls while waiting for --pc-breakpoint (default: 0.02).')

    args = parser.parse_args()

    print(f"RAM Dump: 0x{args.addr:08X} - 0x{args.addr + args.size:08X} ({args.size} bytes)")

    dolphin_proc = None

    # Start Dolphin if --exec provided.
    # IMPORTANT: Prefer this flow over manually launching Dolphin; it reliably
    # opens the GDB stub and avoids "connection refused".
    if args.exec_file:
        config_overrides = list(args.dolphin_config or [])
        if args.enable_mmu:
            # Dolphin stores this in Dolphin.ini under the [Core] section as "MMU".
            # Use the short "Section.Key=Value" form that Dolphin's -C understands.
            config_overrides.append("Core.MMU=True")

        dolphin_proc = start_dolphin(
            args.exec_file,
            args.dolphin,
            user_dir=args.dolphin_userdir,
            config_overrides=config_overrides,
        )
        if dolphin_proc is None:
            sys.exit(1)
        print(f"Waiting {args.delay}s for Dolphin to initialize...")
        time.sleep(args.delay)

    gdb = DolphinGDB(args.host, args.port)

    # Retry connection a few times if we just started Dolphin
    connected = False
    retries = 5 if dolphin_proc else 1
    for i in range(retries):
        if gdb.connect():
            connected = True
            break
        if i < retries - 1:
            print(f"Retrying in 1s... ({i+1}/{retries})")
            time.sleep(1)

    if not connected:
        if dolphin_proc:
            dolphin_proc.terminate()
        sys.exit(1)

    try:
        if args.breakpoint is not None and args.pc_breakpoint is not None:
            print("Choose either --breakpoint (Z0/Z1) or --pc-breakpoint (polling), not both.")
            sys.exit(2)

        if args.breakpoint is not None:
            bp = args.breakpoint
            print(f"Setting breakpoint @ 0x{bp:08X} ...")
            if not gdb.set_sw_breakpoint(bp):
                print("Failed to set breakpoint (stub may not support Z0).")
                sys.exit(3)
            print(f"Continuing until breakpoint (timeout {args.bp_timeout}s) ...")
            gdb.cont()
            stop = gdb.wait_stop(timeout=args.bp_timeout)
            # Ensure we're halted/stable for the dump.
            gdb.halt()
            # Best-effort cleanup.
            gdb.clear_sw_breakpoint(bp)
            if stop is None:
                print("Breakpoint not hit before timeout.")
                sys.exit(4)
            print(f"Stopped: {stop.decode(errors='replace')}")

        elif args.pc_breakpoint is not None:
            target = args.pc_breakpoint
            print(f"Polling until PC==0x{target:08X} (timeout {args.pc_timeout}s, step {args.pc_step}s) ...")
            end = time.time() + args.pc_timeout
            last_pc = None
            hit = False
            while time.time() < end:
                stop = gdb.run_and_halt(duration=args.pc_step)
                pc = parse_stop_pc(stop)
                if pc is not None:
                    last_pc = pc
                    if pc == target:
                        hit = True
                        break
            if not hit:
                if last_pc is None:
                    print("PC checkpoint not hit (no PC decoded from stop packets).")
                else:
                    print(f"PC checkpoint not hit. Last observed PC=0x{last_pc:08X}")
                sys.exit(4)
            print("PC checkpoint hit; dumping.")

        elif args.run > 0:
            # In debugger mode Dolphin can start paused; explicitly continue.
            # (If it was already running, 'c' is harmless.)
            print(f"Running emulation for {args.run}s then halting...")
            gdb.cont()
            time.sleep(args.run)
            gdb.halt()
        elif args.halt:
            print("Halting CPU...")
            gdb.halt()

        print(f"Reading {args.size} bytes from 0x{args.addr:08X}...")

        out_dir = os.path.dirname(os.path.abspath(args.out)) or "."
        os.makedirs(out_dir, exist_ok=True)
        tmp_path = None
        try:
            fd, tmp_path = tempfile.mkstemp(prefix=".ram_dump.", dir=out_dir)
            with os.fdopen(fd, "wb") as f:
                ok = gdb.read_memory_to_file(args.addr, args.size, f, chunk_size=args.chunk)
            if not ok:
                print("Failed to read memory")
                sys.exit(1)
            os.replace(tmp_path, args.out)
            tmp_path = None
        finally:
            if tmp_path and os.path.exists(tmp_path):
                try:
                    os.unlink(tmp_path)
                except Exception:
                    pass

        print(f"Wrote {args.size} bytes to {args.out}")

        if args.cont:
            print("Continuing CPU...")
            gdb.cont()

    finally:
        gdb.close()
        if dolphin_proc:
            print("Terminating Dolphin...")
            dolphin_proc.terminate()
            try:
                dolphin_proc.wait(timeout=5)
            except Exception:
                dolphin_proc.kill()
                dolphin_proc.wait()


if __name__ == '__main__':
    main()
