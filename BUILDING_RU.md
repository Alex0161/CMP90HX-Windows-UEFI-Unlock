# Сборка CMP90HX Windows UEFI Unlock

Эта инструкция относится к ручной версии **MANUAL-4 ALL-ORIGINAL-V2**, где GPU1–GPU4 обрабатываются одинаковым original-v2 алгоритмом.

## Требования

Рекомендуемая среда сборки: Ubuntu/Debian или WSL2.

Нужны:

- GCC;
- GNU binutils с поддержкой `i386pep`;
- GNU make;
- Python 3.

Для Ubuntu/Debian:

```bash
sudo apt update
sudo apt install -y build-essential binutils python3
```

Проверка PE/COFF target:

```bash
ld -V | grep i386pep
```

В выводе должен присутствовать `i386pep`.

## Структура

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
  nvpermissive-core.o   <- пользователь добавляет отдельно
Makefile
```

## Внешний NVPermissive core

Файл `nvpermissive-core.o` не распространяется в этом репозитории из-за отдельно не подтверждённых условий его распространения.

Поместите ваш легально полученный файл сюда:

```text
vendor/nvpermissive-core.o
```

SHA-256 core, использованного при проверенной сборке:

```text
c9702b4887d397272f86dcc25eea2bb11a46d636c91311d7b71f2fb8b01951e5
```

Проверьте:

```bash
sha256sum vendor/nvpermissive-core.o
make verify
```

## Тест ELF loader

```bash
make test
```

Ожидается успешная загрузка core и вывод версии NVPermissive.

## Сборка EFI

```bash
make clean
make
```

Результат:

```text
build/NVPermissiveEFI.efi
```

Для загрузочной FAT32-флешки скопируйте его как:

```text
EFI/BOOT/BOOTX64.EFI
```

## Проверка EFI

```bash
python3 tools/verify_efi.py build/NVPermissiveEFI.efi
```

## Проверенная ручная последовательность

После полного выключения питания:

```text
1 -> GPU1 PASS
2 -> GPU2 PASS
3 -> GPU3 PASS
4 -> GPU4 PASS
C -> 4/4 PASS
W -> Windows
```

Между картами нельзя делать reboot/reset/power-cycle.

Контрольные значения:

```text
SS0 = 0x88888888
SS1 = 0x00000008
GFX = 0x00000004
```

## Проверка в Windows

```powershell
nvidia-smi
nvidia-smi -L
nvidia-smi --query-gpu=index,name,pci.bus_id,memory.total,driver_version --format=csv
```

Проверенная система увидела 4× CMP 90HX по 10240 MiB в TCC и RTX 3060 отдельно.

## Важно

Это экспериментальная pre-boot работа с PCIe/UEFI. На другой материнской плате порядок мостов, Bus ID и поведение SBR могут отличаться.

Перед распространением бинарного EFI прочитайте:

- `SOURCES_AND_LICENSES_RU_EN.md`;
- `THIRD_PARTY_NOTICES.md`.
