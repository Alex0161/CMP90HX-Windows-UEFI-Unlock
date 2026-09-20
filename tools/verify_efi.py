#!/usr/bin/env python3
import struct
import sys
from pathlib import Path


def fail(message: str) -> None:
    raise SystemExit(f"EFI verification failed: {message}")


def main() -> None:
    if len(sys.argv) != 2:
        fail("usage: verify_efi.py FILE")
    data = Path(sys.argv[1]).read_bytes()
    if data[:2] != b"MZ":
        fail("missing DOS/PE signature")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        fail("missing PE signature")
    machine, sections, _time, _sym, _nsyms, opt_size, _chars = struct.unpack_from("<HHIIIHH", data, pe + 4)
    if machine != 0x8664:
        fail(f"machine is 0x{machine:04x}, not x86-64")
    opt = pe + 24
    magic = struct.unpack_from("<H", data, opt)[0]
    if magic != 0x20B:
        fail(f"optional header magic is 0x{magic:04x}, not PE32+")
    subsystem = struct.unpack_from("<H", data, opt + 68)[0]
    if subsystem != 10:
        fail(f"subsystem is {subsystem}, not EFI application (10)")
    entry_rva = struct.unpack_from("<I", data, opt + 16)[0]
    if not entry_rva or not sections or opt_size < 112:
        fail("invalid entry point or section table")
    print(f"EFI OK: size={len(data)}, sections={sections}, entry_rva=0x{entry_rva:x}")


if __name__ == "__main__":
    main()
