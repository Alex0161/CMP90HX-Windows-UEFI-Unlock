# v0.1.0 — Manual 4-GPU verified build

First project release candidate for the verified Windows UEFI workflow.

## Highlights

- Manual one-GPU-at-a-time menu for 4× NVIDIA CMP 90HX.
- GPU1, GPU2, GPU3, and GPU4 use the same original-v2 core path.
- No GPU4-specific CPUCTL_ALIAS in this release.
- Explicit `SS0 / SS1 / GFX` state checks.
- `C` verifies all four cards before boot.
- `W` manually chainloads Windows without another POST/reset.
- Fail-closed behavior if the expected state is not present.
- Tested with 4× CMP 90HX 10 GB + RTX 3060 12 GB.
- Windows driver test: NVIDIA 591.86; all four CMP cards initialized in TCC and reported 10240 MiB each.

## Verified sequence

```text
Power ON
1 -> GPU1 PASS
2 -> GPU2 PASS
3 -> GPU3 PASS
4 -> GPU4 PASS
C -> 4/4 PASS
W -> Windows
```

Expected state:

```text
SS0 = 0x88888888
SS1 = 0x00000008
GFX = 0x00000004
```

## Build inputs

The repository publishes the wrapper/menu source code but does **not** redistribute `nvpermissive-core.o`.

Verified core SHA-256:

```text
c9702b4887d397272f86dcc25eea2bb11a46d636c91311d7b71f2fb8b01951e5
```

Verified `BOOTX64.EFI` SHA-256 from the tested build:

```text
8efc4df0d0344d78af3479b2612b9f4ede9dffe0b239e23c09eebbd26132bef0
```

The tested EFI binary is not attached to the source-only release because it embeds the external core whose standalone redistribution terms have not been independently confirmed.

## Documentation

- `README.md`
- `BUILDING_RU.md`
- `BUILDING_EN.md`
- `SOURCES_AND_LICENSES_RU_EN.md`
- `THIRD_PARTY_NOTICES.md`

## Warning

Experimental hardware research. UEFI/PCIe reset behavior is platform-dependent and may hang or reboot unsupported configurations.
