#!/usr/bin/env python3
import hashlib
import struct
import sys
from pathlib import Path

EXPECTED_SHA256 = "c9702b4887d397272f86dcc25eea2bb11a46d636c91311d7b71f2fb8b01951e5"
ALLOWED_RELOCS = {1, 2, 4, 11}  # R_X86_64_64, PC32, PLT32, 32S
ALLOWED_UNDEFINED = {b"ioread32", b"iowrite32", b"memcpy", b"memcpy_toio"}
REQUIRED_EXPORTS = {
    b"do_permissive",
    b"ga102_v67_open_plm",
    b"pcie_gen2_check_post_reset_gate",
    b"pcie_gen2_find_first_closed_plm",
    b"pcie_gen2_restore_post_reset_group",
    b"pcie_gen2_run_pre_reset_group",
}


def fail(message: str) -> None:
    raise SystemExit(f"core verification failed: {message}")


def main() -> None:
    if len(sys.argv) != 2:
        fail("usage: verify_core.py FILE")
    data = Path(sys.argv[1]).read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest != EXPECTED_SHA256:
        fail(f"SHA-256 {digest} does not match pinned core")
    if data[:4] != b"\x7fELF" or data[4:7] != b"\x02\x01\x01":
        fail("not ELF64 little-endian")
    e_type, e_machine = struct.unpack_from("<HH", data, 16)
    if (e_type, e_machine) != (1, 62):
        fail(f"expected x86-64 relocatable ELF, got type={e_type} machine={e_machine}")
    e_shoff = struct.unpack_from("<Q", data, 40)[0]
    e_shentsize, e_shnum = struct.unpack_from("<HH", data, 58)
    if e_shentsize != 64 or not 1 <= e_shnum <= 128:
        fail("invalid section table")
    sections = [struct.unpack_from("<IIQQQQIIQQ", data, e_shoff + i * 64)
                for i in range(e_shnum)]
    symtab_index = next((i for i, s in enumerate(sections) if s[1] == 2), None)
    if symtab_index is None:
        fail("missing symbol table")
    symtab = sections[symtab_index]
    strtab = sections[symtab[6]]
    strings = data[strtab[4]:strtab[4] + strtab[5]]
    undefined = set()
    exports = set()
    for off in range(symtab[4], symtab[4] + symtab[5], symtab[9]):
        st_name, st_info, _other, st_shndx, _value, _size = struct.unpack_from("<IBBHQQ", data, off)
        end = strings.find(b"\0", st_name)
        name = strings[st_name:end] if end >= 0 else b""
        if st_shndx == 0 and name:
            undefined.add(name)
        elif name:
            exports.add(name)
    if undefined != ALLOWED_UNDEFINED:
        fail(f"unexpected undefined symbols: {sorted(undefined)}")
    missing_exports = REQUIRED_EXPORTS - exports
    if missing_exports:
        fail(f"required exports missing: {sorted(missing_exports)}")
    reloc_types = set()
    for section in sections:
        if section[1] != 4:  # SHT_RELA
            continue
        for off in range(section[4], section[4] + section[5], section[9]):
            _r_offset, r_info, _addend = struct.unpack_from("<QQq", data, off)
            reloc_types.add(r_info & 0xFFFFFFFF)
    if not reloc_types <= ALLOWED_RELOCS:
        fail(f"unsupported relocations: {sorted(reloc_types - ALLOWED_RELOCS)}")
    print(f"core OK: sha256={digest}, sections={e_shnum}, relocs={sorted(reloc_types)}")


if __name__ == "__main__":
    main()
