#!/usr/bin/env python3
import argparse
import json
import random
from pathlib import Path


def pick_case(rng: random.Random) -> tuple[int, int]:
    # 70% trace-biased around observed retail usage (chan=0, cmd=2).
    # 20% boundary-valid.
    # 10% exploratory-valid.
    x = rng.random()
    if x < 0.7:
        chan = 0 if rng.random() < 0.85 else rng.randint(0, 3)
        cmd = 2 if rng.random() < 0.85 else rng.choice([0, 1, 2])
        return chan, cmd
    if x < 0.9:
        boundary = [
            (0, 0),
            (0, 2),
            (3, 0),
            (3, 2),
            (1, 1),
            (2, 1),
        ]
        return boundary[rng.randrange(len(boundary))]
    return rng.randint(0, 3), rng.choice([0, 1, 2])


def main() -> int:
    ap = argparse.ArgumentParser(description="Generate trace-guided PADControlMotor cases")
    ap.add_argument("--seed", type=lambda x: int(x, 0), default=0xC0DE1234)
    ap.add_argument("--count", type=int, default=128)
    ap.add_argument(
        "--out",
        type=Path,
        default=Path("tests/trace-guided/pad_control_motor/cases/seed_0xC0DE1234_cases.jsonl"),
    )
    args = ap.parse_args()

    rng = random.Random(args.seed)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open("w", encoding="utf-8") as f:
        for i in range(args.count):
            chan, cmd = pick_case(rng)
            rec = {
                "case_id": f"tg_pad_control_motor_{i:04d}",
                "seed": f"0x{args.seed:08X}",
                "chan": chan,
                "cmd": cmd,
                "expected_motor_cmd": cmd,
            }
            f.write(json.dumps(rec, separators=(",", ":")) + "\n")

    print(f"Wrote {args.count} cases -> {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
