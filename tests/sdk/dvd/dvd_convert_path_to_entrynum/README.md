# dvd_convert_path_to_entrynum

MP4 callsite-driven tests for `DVDConvertPathToEntrynum`.

Note: we do not model the real disc FST yet. The DOL uses a local stub as an
oracle; the host uses `src/sdk_port/dvd/DVD.c` with an injectable test path map
(`gc_dvd_test_set_paths`) to match the oracle deterministically.

