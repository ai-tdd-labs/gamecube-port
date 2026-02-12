# PADControlMotor trace-guided pilot

This is the first trace-guided constrained-random pilot suite.

Inputs are generated from a model derived from harvested traces:
- model: `tests/trace-guided/pad_control_motor/model.json`
- generated cases: `tests/trace-guided/pad_control_motor/cases/*.jsonl`

Run:

```bash
tools/replay_trace_guided_pad_control_motor.sh --seed 0xC0DE1234 --count 128
```

Run against Dolphin oracle batch:

```bash
tools/replay_trace_guided_pad_control_motor.sh --oracle dolphin --seed 0xC0DE1234 --count 64
```

Run high-coverage Dolphin suites:

```bash
# exhaustive matrix (invalid+valid channels, command edges)
tools/replay_trace_guided_pad_control_motor.sh --oracle dolphin --dolphin-suite exhaustive_matrix_001

# unified L0-L5 DOL PBT suite (isolated/accumulation/overwrite/random-start/harvest/boundary)
tools/replay_trace_guided_pad_control_motor.sh --oracle dolphin --dolphin-suite pbt_001

# large trace-guided batch (2048 deterministic cases)
tools/replay_trace_guided_pad_control_motor.sh --oracle dolphin --dolphin-suite trace_guided_batch_002 --seed 0xC0DE1234 --count 2048

# run all Dolphin suites in one go
tools/replay_trace_guided_pad_control_motor.sh --oracle dolphin --dolphin-suite max
```

Notes:
- Current retail harvest for this function is sparse (mostly `chan=0, cmd=2`).
- Generator is therefore strongly trace-biased but still explores valid channels
  (`0..3`) and motor commands (`0,1,2`).
- Dolphin batch mode currently uses fixed seed/count (`0xC0DE1234`, `64`) to keep
  DOL and host generators byte-identical.
- `trace_guided_batch_002` is the extended deterministic stress set (`2048` cases).
