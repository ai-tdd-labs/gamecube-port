#!/usr/bin/env python3
"""
Write deterministic controller commands into a Dolphin "Pipe" input FIFO.

Dolphin supports a Pipe input backend that reads newline-delimited commands:
  PRESS {A,B,X,Y,Z,START,L,R,D_UP,D_DOWN,D_LEFT,D_RIGHT}
  RELEASE {same}
  SET {L,R} [0..1]
  SET {MAIN,C} [0..1] [0..1]

We use this to automate RVZ menu navigation in headless runs, without recording a .dtm.
"""

from __future__ import annotations

import argparse
import os
import time


def parse_float(s: str) -> float:
    return float(s)


def send(f, line: str) -> None:
    if not line.endswith("\n"):
        line += "\n"
    f.write(line.encode("ascii"))
    f.flush()


def press(f, tok: str, hold_s: float) -> None:
    send(f, f"PRESS {tok}")
    time.sleep(max(0.0, hold_s))
    send(f, f"RELEASE {tok}")


def main() -> None:
    ap = argparse.ArgumentParser(description="Write deterministic commands to a Dolphin Pipe input FIFO")
    ap.add_argument("--fifo", required=True, help="Path to FIFO file in Dolphin user Pipes dir")
    ap.add_argument("--hold", type=parse_float, default=0.06, help="Default button hold duration (seconds)")
    ap.add_argument("--boot-wait", type=parse_float, default=10.0, help="Initial wait before sending inputs")
    ap.add_argument("--duration", type=parse_float, default=180.0, help="Total time to emit inputs (seconds)")
    ap.add_argument("--period", type=parse_float, default=0.55, help="Base period for repeating inputs (seconds)")
    args = ap.parse_args()

    fifo = args.fifo
    if not os.path.exists(fifo):
        raise SystemExit(f"fatal: fifo not found: {fifo}")

    # Open write-end. This may block until Dolphin opens the read-end; that's intended.
    with open(fifo, "wb", buffering=0) as f:
        time.sleep(max(0.0, args.boot_wait))

        start = time.time()
        n = 0
        while True:
            now = time.time()
            if now - start >= args.duration:
                break

            # Deterministic "menu mash" script:
            # - periodic START/A presses
            # - occasional D_DOWN to move selection
            # This is intentionally simple; the goal is to reach *some* overlay transition.
            if n % 4 == 0:
                press(f, "START", args.hold)
            else:
                press(f, "A", args.hold)

            if n % 7 == 0:
                press(f, "D_DOWN", args.hold)
            if n % 13 == 0:
                press(f, "D_RIGHT", args.hold)

            time.sleep(max(0.0, args.period))
            n += 1


if __name__ == "__main__":
    main()

