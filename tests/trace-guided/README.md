# Trace-Guided Constrained-Random Artifacts

This directory stores artifacts derived from `tests/trace-harvest/` for
high-volume deterministic case generation.

Purpose:
- keep raw retail captures separate from generated/randomized case batches
- make replay inputs reproducible (`seed`, `case_id`, fixed schema)
- feed both DOL-side and host-side runners with the same case stream

Layout (planned):
- `<function>/model.json` — constraints from harvested traces (ranges, enums, state rules)
- `<function>/cases/seed_<seed>.bin` — generated deterministic case batch
- `<function>/reports/*.json` — generation summaries and mismatch reports

Notes:
- Raw traces remain source-of-truth in `tests/trace-harvest/`.
- This directory is for derived data and generator outputs only.
