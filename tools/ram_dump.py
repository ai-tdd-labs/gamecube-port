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


class DolphinGDB:
    """Simple GDB client for Dolphin's GDB stub."""

    def __init__(self, host='127.0.0.1', port=9090):
        self.host = host
        self.port = port
        self.sock = None

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

    def _send(self, cmd):
        """Send GDB command with checksum."""
        checksum = sum(cmd.encode()) % 256
        packet = f"${cmd}#{checksum:02x}"
        self.sock.send(packet.encode())

    def _recv(self, timeout=5):
        """Receive GDB response."""
        self.sock.settimeout(timeout)
        data = b""
        start = time.time()

        while time.time() - start < timeout:
            try:
                chunk = self.sock.recv(4096)
                if not chunk:
                    break
                data += chunk

                # Check for complete packet: $<data>#XX
                if b'$' in data and b'#' in data:
                    start_idx = data.find(b'$')
                    hash_idx = data.find(b'#', start_idx)
                    if hash_idx >= 0 and len(data) >= hash_idx + 3:
                        # Send ACK
                        self.sock.send(b'+')
                        return data
            except socket.timeout:
                if data:
                    continue
                break

        return data

    def read_memory(self, addr, size):
        """Read memory from Dolphin. Returns hex string or None."""
        self._send(f'm{addr:x},{size:x}')
        resp = self._recv()

        if resp and b'+$' in resp:
            # Extract hex data between $ and #
            start = resp.find(b'$') + 1
            end = resp.find(b'#')
            hex_data = resp[start:end].decode(errors="replace").strip()

            # Check for error response
            if hex_data.startswith('E'):
                return None
            # Defensive: ignore malformed replies (we rely on bytes.fromhex later).
            # This happens rarely when the TCP stream contains interleaved/non-hex data.
            for ch in hex_data:
                if ch not in "0123456789abcdefABCDEF":
                    return None
            return hex_data
        return None

    def read_memory_bytes(self, addr, size, chunk_size=0x1000):
        """Read memory and return as bytes. Handles chunked reads."""
        data = bytearray()
        remaining = size
        cursor = addr

        while remaining > 0:
            chunk = min(remaining, chunk_size)
            hex_data = self.read_memory(cursor, chunk)

            if not hex_data:
                # Try smaller chunks
                if chunk_size > 0x100:
                    chunk_size = chunk_size // 2
                    continue
                print(f"Failed to read at 0x{cursor:08X}")
                return None

            data.extend(bytes.fromhex(hex_data))
            remaining -= chunk
            cursor += chunk

            # Progress indicator
            pct = ((size - remaining) / size) * 100
            print(f"\rReading: {pct:.1f}%", end="", flush=True)

        print()  # newline after progress
        return bytes(data)

    def halt(self):
        """Halt CPU execution."""
        self.sock.send(b'\x03')
        self._recv(timeout=2)
        time.sleep(0.1)

    def cont(self):
        """Continue CPU execution."""
        self._send('c')

    def run_and_halt(self, duration=0.5):
        """Continue execution for a duration then halt."""
        self.cont()
        time.sleep(duration)
        self.halt()

    def close(self):
        if self.sock:
            self.sock.close()


def parse_int(s):
    """Parse int from string (supports 0x prefix)."""
    return int(s, 0)


# Default Dolphin path on macOS
DOLPHIN_BIN = "/Applications/Dolphin.app/Contents/MacOS/Dolphin"


def start_dolphin(exec_path, dolphin_bin=DOLPHIN_BIN):
    """Start Dolphin in headless mode with GDB stub enabled."""
    if not os.path.isfile(dolphin_bin):
        print(f"Dolphin not found at {dolphin_bin}")
        return None

    if not os.path.isfile(exec_path):
        print(f"File not found: {exec_path}")
        return None

    # Use --exec flow so Dolphin starts headless with GDB stub active.
    # -b = batch (headless), -d = debugger (required for stub), -e = execute
    cmd = [dolphin_bin, "-b", "-d", "-e", exec_path]
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

    args = parser.parse_args()

    print(f"RAM Dump: 0x{args.addr:08X} - 0x{args.addr + args.size:08X} ({args.size} bytes)")

    dolphin_proc = None

    # Start Dolphin if --exec provided.
    # IMPORTANT: Prefer this flow over manually launching Dolphin; it reliably
    # opens the GDB stub and avoids "connection refused".
    if args.exec_file:
        dolphin_proc = start_dolphin(args.exec_file, args.dolphin)
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
        if args.run > 0:
            print(f"Running emulation for {args.run}s then halting...")
            gdb.run_and_halt(args.run)
        elif args.halt:
            print("Halting CPU...")
            gdb.halt()

        print(f"Reading {args.size} bytes from 0x{args.addr:08X}...")
        data = gdb.read_memory_bytes(args.addr, args.size)

        if data is None:
            print("Failed to read memory")
            sys.exit(1)

        if len(data) != args.size:
            print(f"Warning: Got {len(data)} bytes, expected {args.size}")

        with open(args.out, 'wb') as f:
            f.write(data)

        print(f"Wrote {len(data)} bytes to {args.out}")

        if args.cont:
            print("Continuing CPU...")
            gdb.cont()

    finally:
        gdb.close()
        if dolphin_proc:
            print("Terminating Dolphin...")
            dolphin_proc.terminate()
            dolphin_proc.wait(timeout=5)


if __name__ == '__main__':
    main()
