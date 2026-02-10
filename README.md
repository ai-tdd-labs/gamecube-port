# GameCube SDK Port (Evidence-Only, Deterministic)

This repo is an AI-assisted reverse-engineering + porting project for Nintendo
GameCube games.

The core idea is simple:

- We **do not guess** SDK behavior.
- We **discover** behavior from evidence (decomp / disassembly / Dolphin runtime).
- We lock it in with **deterministic tests** until **expected == actual** (bit-exact).

Codex has no memory.
The repository is the memory.

## Scope

In scope:
- Nintendo GameCube **SDK** behavior (OS/DVD/GX/VI/PAD/SI/EXI/etc).
- Deterministic unit tests that validate SDK behavior.
- Small game-derived workloads that exercise SDK call chains (MP4-first, but multi-game).

Out of scope (not published here):
- ROMs / ISOs / RVZs and other copyrighted game content.
- Large RAM dumps (expected/actual `.bin` files are generated locally and ignored by git).
- Full game source for commercial titles (we only use locally-available decomp trees as evidence).

## Repository Layout (High Level)

- `src/sdk_port/`
  - Host-side reimplementation of SDK behavior.
  - These functions are the thing we are making correct.
- `tests/sdk/`
  - One SDK function per folder.
  - Each suite has:
    - a PPC DOL test (run in Dolphin) producing `expected/*.bin`
    - a host scenario producing `actual/*.bin`
    - a diff step that must PASS bit-exact
- `tests/sdk/smoke/`
  - "Chain" smoke tests that exercise a fixed SDK call sequence and assert combined effects.
- `tests/workload/`
  - Small integration workloads (game-derived call chains) built for the host runner.
- `tools/`
  - `ram_dump.py` talks to Dolphin's GDB stub and dumps RAM regions.
  - scripts to run host scenarios and diff expected vs actual.
- `docs/codex/`
  - The project's "memory": workflow, decisions, glossary, facts-only notes.

## How Correctness Works

Every SDK behavior claim is backed by one of:
- decompiled source (local, not committed)
- disassembly / symbol evidence
- Dolphin behavior (RAM/register state at a precise checkpoint)

And then enforced by tests:

1. **expected**: run a PPC DOL in Dolphin and dump RAM (`expected.bin`)
2. **actual**: run the equivalent call on the host SDK port and dump RAM (`actual.bin`)
3. Diff must be bit-exact (`PASS`)

For retail semantics we also use an optional second oracle:
- run the real game image locally in Dolphin, stop at a checkpoint PC, dump RAM.

## Harvesting Retail Testcases (RVZ Breakpoints)

Besides "PPC DOL vs host" tests, we can also **harvest real-game call instances** from a retail
RVZ/ISO without modifying the game:

1. Run retail RVZ in Dolphin headless with the GDB stub
2. Break at a target function's entry PC (from `symbols.txt`)
3. Read LR and set a temporary breakpoint at the return address
4. Record a small snapshot (registers + a RAM window) at entry and exit
5. Dedupe unique inputs and write an index (`trace.jsonl`)

Tool:
- `tools/trace_pc_entry_exit.py`

These traces are local-only (`tests/traces/**` is gitignored). We then:
- derive invariants (e.g. MSR bits and return values for `OSDisableInterrupts`)
- and/or convert representative cases into deterministic PPC-vs-host unit tests

## Quickstart (Run One Test)

Prereqs:
- macOS (current scripts assume macOS paths)
- Dolphin installed as `/Applications/Dolphin.app`
- devkitPPC installed (to build DOL tests)

Example: run one GX unit test (expected vs actual):

```bash
cd <REPO_ROOT>

# Produce expected.bin (PPC DOL in Dolphin -> RAM dump)
tools/dump_expected.sh \
  tests/sdk/gx/gx_init_tex_obj_lod/dol/mp4/realistic_pfdrawfonts_001/gx_init_tex_obj_lod_realistic_pfdrawfonts_001.dol \
  tests/sdk/gx/gx_init_tex_obj_lod/expected/gx_init_tex_obj_lod_mp4_pfdrawfonts_001.bin \
  0x80300000 0x40 0.5

# Produce actual.bin (host SDK port)
tools/run_host_scenario.sh \
  tests/sdk/gx/gx_init_tex_obj_lod/host/gx_init_tex_obj_lod_mp4_pfdrawfonts_001_scenario.c

# Diff
tools/diff_bins.sh \
  tests/sdk/gx/gx_init_tex_obj_lod/expected/gx_init_tex_obj_lod_mp4_pfdrawfonts_001.bin \
  tests/sdk/gx/gx_init_tex_obj_lod/actual/gx_init_tex_obj_lod_mp4_pfdrawfonts_001.bin
```

To run a whole batch:

```bash
tools/run_tests.sh
```

## Decomp Trees / External Dependencies

This repo expects you to have local decomp trees available for evidence.
Do not commit them here.

Two supported approaches:

1. **Git submodules (recommended)**:
   - This repo uses submodules under `external/`.
   - Initialize them after clone:

     ```bash
     git submodule update --init --recursive
     ```

2. **Symlinks (supported)**:
   - The repo root contains symlinks like `decomp_mario_party_4 -> external/mp4-decomp`.
   - If you keep your own local trees elsewhere, you can repoint the symlinks locally.

The docs use placeholders like `<DECOMP_MP4_ROOT>` and `<MP4_RVZ_PATH>` so the repo
is publishable without containing your local paths.

## Workflow (Read This First)

See:
- `docs/codex/WORKFLOW.md`
- `docs/codex/NOTES.md`
- `docs/codex/DECISIONS.md`

## License / Legal

This repo is intended to be publishable without copyrighted game assets.
Do not add ROMs, ISOs, RVZs, or proprietary decomp sources to git.
