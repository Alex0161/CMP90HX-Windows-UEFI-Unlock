# Building CMP90HX Windows UEFI Unlock

This guide applies to the **MANUAL-4 ALL-ORIGINAL-V2** build where GPU1–GPU4 use the same original-v2 algorithm.

## Requirements

Recommended build environment: Ubuntu/Debian or WSL2.

Required tools:

- GCC;
- GNU binutils with `i386pep` support;
- GNU make;
- Python 3.

Ubuntu/Debian:

```bash
sudo apt update
sudo apt install -y build-essential binutils python3
```

Verify PE/COFF target support:

```bash
ld -V | grep i386pep
```

The output must include `i386pep`.

## Source layout

```text
src/
  main.c
  elf_loader.c
include/
  core_abi.h
  elf64_min.h
  elf_loader.h
  uefi_min.h
tools/
  verify_core.py
  verify_efi.py
tests/
  test_elf_loader.c
vendor/
  nvpermissive-core.o   <- supplied separately by the user
Makefile
```

## External NVPermissive core

`nvpermissive-core.o` is intentionally not redistributed in this repository because its standalone redistribution terms have not been independently confirmed.

Place your legally obtained copy at:

```text
vendor/nvpermissive-core.o
```

SHA-256 of the core used for the verified build:

```text
c9702b4887d397272f86dcc25eea2bb11a46d636c91311d7b71f2fb8b01951e5
```

Verify it:

```bash
sha256sum vendor/nvpermissive-core.o
make verify
```

## ELF loader test

```bash
make test
```

The loader should successfully load the core and print the NVPermissive version.

## Build the EFI application

```bash
make clean
make
```

Output:

```text
build/NVPermissiveEFI.efi
```

For a FAT32 boot drive, copy it to:

```text
EFI/BOOT/BOOTX64.EFI
```

## Verify the EFI image

```bash
python3 tools/verify_efi.py build/NVPermissiveEFI.efi
```

## Verified manual sequence

After a full power-off:

```text
1 -> GPU1 PASS
2 -> GPU2 PASS
3 -> GPU3 PASS
4 -> GPU4 PASS
C -> 4/4 PASS
W -> Windows
```

Do not reboot, reset, or power-cycle between cards.

Expected register values:

```text
SS0 = 0x88888888
SS1 = 0x00000008
GFX = 0x00000004
```

## Windows verification

```powershell
nvidia-smi
nvidia-smi -L
nvidia-smi --query-gpu=index,name,pci.bus_id,memory.total,driver_version --format=csv
```

The verified system initialized 4× CMP 90HX with 10240 MiB each in TCC mode, plus a separate RTX 3060.

## Important

This is experimental pre-boot PCIe/UEFI work. Bridge topology, Bus IDs, and SBR behavior may differ on other motherboards.

Before redistributing any built EFI image, read:

- `SOURCES_AND_LICENSES_RU_EN.md`;
- `THIRD_PARTY_NOTICES.md`.
