#!/usr/bin/env python3
"""
RAM Dump Tool for Dolphin Emulator

Connects to Dolphin's GDB stub and dumps RAM to a file.
Used for creating expected.bin files from Dolphin for unit tests.

Usage:
    python3 ram_dump.py --addr 0x80300000 --size 4096 --out expected.bin
    python3 ram_dump.py --addr 0x80300000 --size 0x1000 --out expected.bin

Prerequisites:
    1. Start Dolphin with a test DOL
    2. Enable GDB stub: Options -> Configuration -> Debug -> Enable GDB Stub
    3. Dolphin listens on 127.0.0.1:9090
"""

import socket
import argparse
import sys
import time


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
            hex_data = resp[start:end].decode()

            # Check for error response
            if hex_data.startswith('E'):
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

    def close(self):
        if self.sock:
            self.sock.close()


def parse_int(s):
    """Parse int from string (supports 0x prefix)."""
    return int(s, 0)


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
    parser.add_argument('--host', default='127.0.0.1',
                        help='Dolphin GDB host (default: 127.0.0.1)')
    parser.add_argument('--port', type=int, default=9090,
                        help='Dolphin GDB port (default: 9090)')
    parser.add_argument('--halt', action='store_true',
                        help='Halt CPU before reading (for stable state)')
    parser.add_argument('--continue', dest='cont', action='store_true',
                        help='Continue CPU after reading')

    args = parser.parse_args()

    print(f"RAM Dump: 0x{args.addr:08X} - 0x{args.addr + args.size:08X} ({args.size} bytes)")

    gdb = DolphinGDB(args.host, args.port)

    if not gdb.connect():
        sys.exit(1)

    try:
        if args.halt:
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


if __name__ == '__main__':
    main()
