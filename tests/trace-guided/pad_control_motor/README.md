# PADControlMotor trace-guided pilot

This is the first trace-guided constrained-random pilot suite.

Inputs are generated from a model derived from harvested traces:
- model: `tests/trace-guided/pad_control_motor/model.json`
- generated cases: `tests/trace-guided/pad_control_motor/cases/*.jsonl`

Run:

```bash
tools/replay_trace_guided_pad_control_motor.sh --seed 0xC0DE1234 --count 128
```

Notes:
- Current retail harvest for this function is sparse (mostly `chan=0, cmd=2`).
- Generator is therefore strongly trace-biased but still explores valid channels
  (`0..3`) and motor commands (`0,1,2`).
