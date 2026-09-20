# CMP90HX Windows UEFI Unlock — Manual 4-GPU

[Русский](#русский) | [English](#english)

> Experimental UEFI pre-boot unlock workflow for NVIDIA CMP 90HX (GA102, PCI ID 10DE:220D) on Windows.
>
> Экспериментальный UEFI pre-boot unlock для NVIDIA CMP 90HX (GA102, PCI ID 10DE:220D) под Windows.

## Русский

### Проверенная конфигурация

Ручной вариант был проверен на системе:

- 4 × NVIDIA CMP 90HX 10 GB;
- 1 × GeForce RTX 3060 12 GB;
- Windows;
- NVIDIA Driver 591.86;
- CUDA reported by `nvidia-smi`: 13.1.

После ручной последовательности все четыре CMP 90HX одновременно определялись драйвером в режиме TCC, каждая с 10240 MiB VRAM.

### Почему ручной режим

На этой конкретной multi-GPU конфигурации автоматическая обработка всех четырёх карт подряд была менее стабильной. Рабочая схема — обрабатывать каждую CMP отдельно в одном UEFI-сеансе без reboot/POST между картами, затем вручную запускать Windows.

### Меню

```text
1  Unlock GPU1 only
2  Unlock GPU2 only
3  Unlock GPU3 only
4  Unlock GPU4 only
C  Check actual SS0/SS1/GFX state
W  Manually start Windows
H  Halt
```

Все 4 карты используют одинаковый original-v2 путь:

```text
initial dual-SBR
→ compute
→ dual-SBR
→ graphics
→ dual-SBR
→ Windows-boundary verification
```

Специального `CPUCTL_ALIAS` для GPU4 в этой версии нет.

### Рекомендуемый порядок

```text
Power ON
1 → дождаться PASS
2 → дождаться PASS
3 → дождаться PASS
4 → дождаться PASS
C → должно быть 4/4 PASS
W → Windows без дополнительного POST/reset
```

Между `1`, `2`, `3`, `4` нельзя делать reboot/reset/power-cycle.

Контрольные значения после unlock:

```text
SS0 = 0x88888888
SS1 = 0x00000008
GFX = 0x00000004
```

### Проверка в Windows

```powershell
nvidia-smi
nvidia-smi -L
nvidia-smi --query-gpu=index,name,pci.bus_id,memory.total,driver_version --format=csv
```

Далее рекомендуется проверить реальную CUDA-нагрузку отдельно на каждой CMP 90HX.

### Сборка

Сборка ожидает внешний файл:

```text
vendor/nvpermissive-core.o
```

Он намеренно не включён в этот репозиторий, пока условия его отдельного распространения не подтверждены. Ожидаемый SHA-256 использованного core:

```text
c9702b4887d397272f86dcc25eea2bb11a46d636c91311d7b71f2fb8b01951e5
```

После помещения core:

```bash
make verify
make test
make
```

Результат:

```text
build/NVPermissiveEFI.efi
```

См. [SOURCES_AND_LICENSES_RU_EN.md](SOURCES_AND_LICENSES_RU_EN.md) и [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

---

## English

### Verified configuration

The manual build was tested on a system with:

- 4 × NVIDIA CMP 90HX 10 GB;
- 1 × GeForce RTX 3060 12 GB;
- Windows;
- NVIDIA Driver 591.86;
- CUDA reported by `nvidia-smi`: 13.1.

After the manual sequence, all four CMP 90HX cards were simultaneously initialized by the NVIDIA driver in TCC mode with 10240 MiB VRAM each.

### Why manual mode

On this particular multi-GPU platform, automatically processing all four CMP cards in one uninterrupted sequence was less stable. The verified workflow processes one card at a time inside the same UEFI session, without reboot/POST between cards, then chainloads Windows manually.

### Menu

```text
1  Unlock GPU1 only
2  Unlock GPU2 only
3  Unlock GPU3 only
4  Unlock GPU4 only
C  Check actual SS0/SS1/GFX state
W  Manually start Windows
H  Halt
```

All four cards use the same original-v2 path:

```text
initial dual-SBR
→ compute
→ dual-SBR
→ graphics
→ dual-SBR
→ Windows-boundary verification
```

There is no GPU4-specific `CPUCTL_ALIAS` in this version.

### Recommended sequence

```text
Power ON
1 → wait for PASS
2 → wait for PASS
3 → wait for PASS
4 → wait for PASS
C → expect 4/4 PASS
W → chainload Windows without another POST/reset
```

Do not reboot, reset, or power-cycle between `1`, `2`, `3`, and `4`.

Expected unlocked register values:

```text
SS0 = 0x88888888
SS1 = 0x00000008
GFX = 0x00000004
```

### Windows verification

```powershell
nvidia-smi
nvidia-smi -L
nvidia-smi --query-gpu=index,name,pci.bus_id,memory.total,driver_version --format=csv
```

A real CUDA workload on each CMP card is recommended as the final compute test.

### Build

The build expects an externally supplied:

```text
vendor/nvpermissive-core.o
```

It is intentionally not committed here while its separate redistribution terms remain unresolved. Expected SHA-256 for the core used during testing:

```text
c9702b4887d397272f86dcc25eea2bb11a46d636c91311d7b71f2fb8b01951e5
```

Then run:

```bash
make verify
make test
make
```

Output:

```text
build/NVPermissiveEFI.efi
```

See [SOURCES_AND_LICENSES_RU_EN.md](SOURCES_AND_LICENSES_RU_EN.md) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Source archive / Архив исходников

The complete source snapshot used for this tested build is stored as:

`source/CMP90HX_source_only.zip`

It contains the wrapper sources, headers, Makefile, verification tools, tests,
the GPU4-identical patch, and the original Russian notes. The external
`nvpermissive-core.o` is intentionally not included.

Полный снимок исходников, использованный для этой проверенной сборки, находится в:

`source/CMP90HX_source_only.zip`

В архиве находятся исходники оболочки, заголовки, Makefile, инструменты проверки,
тесты, patch для одинаковой обработки GPU4 и исходные русские заметки. Внешний
`nvpermissive-core.o` намеренно не включён.

## Status

This repository is experimental hardware research. Use at your own risk. UEFI/PCIe reset operations can hang or reboot some platforms.

## License

New wrapper/menu code is published under MIT where applicable. Third-party components retain their own licenses and terms. See the provenance and notices files before redistributing binaries.
