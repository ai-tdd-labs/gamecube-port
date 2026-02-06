#!/usr/bin/env python3
import struct
import sys

TEXT_SECTIONS = 7
DATA_SECTIONS = 11

FMT = ">" + "I" * (TEXT_SECTIONS + DATA_SECTIONS)  # offsets


def read_u32s(f, count):
    data = f.read(4 * count)
    return struct.unpack(">" + "I" * count, data)


def main(path):
    with open(path, "rb") as f:
        text_off = read_u32s(f, TEXT_SECTIONS)
        data_off = read_u32s(f, DATA_SECTIONS)
        text_addr = read_u32s(f, TEXT_SECTIONS)
        data_addr = read_u32s(f, DATA_SECTIONS)
        text_size = read_u32s(f, TEXT_SECTIONS)
        data_size = read_u32s(f, DATA_SECTIONS)
        bss_addr, bss_size, entry = read_u32s(f, 3)

    print(f"DOL: {path}")
    print(f"Entry: 0x{entry:08X}")
    print(f"BSS:   0x{bss_addr:08X} size 0x{bss_size:X}")

    for i in range(TEXT_SECTIONS):
        if text_size[i]:
            print(f".text{i}: off 0x{text_off[i]:X} addr 0x{text_addr[i]:08X} size 0x{text_size[i]:X}")

    for i in range(DATA_SECTIONS):
        if data_size[i]:
            print(f".data{i}: off 0x{data_off[i]:X} addr 0x{data_addr[i]:08X} size 0x{data_size[i]:X}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("usage: dol_info.py <file.dol>")
        sys.exit(1)
    main(sys.argv[1])
