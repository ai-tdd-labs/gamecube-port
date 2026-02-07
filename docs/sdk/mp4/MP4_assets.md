# MP4 Assets (Evidence)

This repo treats real-game assets as **secondary oracle** only (best effort). The primary oracle remains deterministic `sdk_port` PPC-vs-host smoke tests.

## Mario Party 4 (USA)

- Path: `/Users/chrislamark/projects/recomp/gamecube_static_recomp/game_files/Mario Party 4 (USA).rvz`
- SHA256: `23bd6d688ff4983b8eb42f48025156e8cec023d52831b0490778415ac85e46d4`

### Real-game dump tooling

Preferred: use `tools/ram_dump.py` with `--breakpoint` (GDB remote `Z0/Z1`) to stop at an exact checkpoint and dump RAM.

Fallback: use `--pc-breakpoint` polling if needed.
