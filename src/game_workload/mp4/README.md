# MP4 Workload (Mario Party 4)

Goal: use MP4 decompiled code as an integration workload to exercise the SDK port.

We do **not** port game code into `src/sdk_port/`. The workload is separate by design.

## Sync policy

By default, we reference the local decomp checkout via the repo symlink:
- `decomp_mario_party_4/` -> external decomp root on this machine

If a local copy is useful for experiments, use `tools/sync_mp4_workload.sh` to copy a
whitelist of files into `src/game_workload/mp4/vendor/` (gitignored by default).

## File list

See `src/game_workload/mp4/filelist.txt` for the current whitelist.

