# Sources, Credits, Licenses, and Redistribution Notes / Источники, авторство, лицензии и условия распространения

> Last reviewed / Последняя проверка: 2026-09-21  
> Scope / Область: CMP 90HX Windows UEFI unlock/manual multi-GPU work and optional BeeLlama/CMP 90HX inference stack / ручной UEFI-unlock CMP 90HX под Windows, multi-GPU и опциональный стек BeeLlama/CMP 90HX.

---

## 1. Important licensing rule / Важное правило лицензирования

**EN:**  
The license of this repository's new wrapper/menu code does not automatically replace the licenses of embedded, derived, or third-party components.

In particular:

- the UEFI wrapper used as the starting point is MIT-licensed;
- `nvpermissive-core.o` is a separate upstream component;
- NVIDIA firmware, drivers, Windows boot files, and vendor binaries are not covered by the wrapper's MIT license;
- GPL-derived code must retain its GPL obligations;
- research papers and documentation may be cited, but citation does not grant permission to redistribute copyrighted text or binaries.

If an upstream license is unclear, treat the component as reference-only until permission or a clear license is confirmed.

**RU:**  
Лицензия нового кода оболочки/меню этого репозитория не заменяет автоматически лицензии встроенных, производных или сторонних компонентов.

В частности:

- UEFI-оболочка, использованная как основа, распространяется по MIT;
- `nvpermissive-core.o` является отдельным сторонним компонентом;
- прошивки NVIDIA, драйверы, загрузочные файлы Windows и другие бинарные файлы производителей не подпадают под MIT-лицензию оболочки;
- код, производный от GPL-проектов, должен сохранять требования GPL;
- научные статьи и документацию можно цитировать, но сама ссылка не даёт права распространять защищённый авторским правом текст или бинарные файлы.

Если лицензия стороннего компонента неясна, безопаснее считать его только исследовательским источником до подтверждения условий.

---

# PART A / ЧАСТЬ A — CMP 90HX UEFI / Windows unlock provenance / происхождение UEFI-unlock для Windows

## 2. Primary wrapper source actually used / Основной реально использованный исходный код

### NVPermissiveEFI Windows Unlock v0.2.1

**EN:**  
Source package used: `NVPermissiveEFI-WindowsUnlock-v0.2.1.zip`.

The package contains wrapper source, ELF loader code, minimal UEFI headers, build tools, `LICENSE`, `THIRD_PARTY_NOTICES.md`, and a built EFI application.

Wrapper/tooling license: **MIT**.

**RU:**  
Использованный исходный пакет: `NVPermissiveEFI-WindowsUnlock-v0.2.1.zip`.

Пакет содержит исходники UEFI-оболочки, ELF loader, минимальные UEFI-заголовки, инструменты сборки, `LICENSE`, `THIRD_PARTY_NOTICES.md` и собранное EFI-приложение.

Лицензия оболочки и вспомогательного кода: **MIT**.

### Multi-GPU derivative / Производная multi-GPU версия

`NVPermissiveEFI-MultiGPU-v0.2.1-mg1.zip`

Derived from the same MIT wrapper, while keeping a separate third-party notice for the embedded core.

### Local modifications / Локальные изменения

- PCI discovery for multiple CMP 90HX cards / обнаружение нескольких CMP 90HX через PCI;
- manual 4-GPU menu / ручное меню на 4 GPU;
- per-GPU unlock selection `1/2/3/4`;
- explicit `SS0`, `SS1`, `GFX` checks;
- manual Windows boot only on user request;
- fail-closed behavior;
- Windows Boot Manager handoff without POST/reset;
- reset-order and multi-GPU sequencing experiments.

Suggested license for new wrapper/menu source only: **MIT**, while preserving original MIT notices and third-party notices.

---

## 3. NVPermissive core object — separate license boundary / NVPermissive core — отдельная лицензионная граница

Component / Компонент: `nvpermissive-core.o`

Observed/pinned SHA-256:

`c9702b4887d397272f86dcc25eea2bb11a46d636c91311d7b71f2fb8b01951e5`

Pinned archive: `nvpermissive-dist-380bdf3.tar.gz`

SHA-256:

`d5e89e77e121e1295cb39d331e0d511275b6edc1d7236376e0fd13ac6f0c79c0`

Upstream location / Источник:

https://alist.homelabproject.cc/foxipan/vGPU/CMP_90HX/GraphicsUnlock

Original attribution / Атрибуция: **GreenDamTan / RainCandyTech**, NVPermissive.

The upstream Linux wrapper declares `MODULE_LICENSE("GPL")`, but the pinned archive used by the Windows wrapper did not contain a standalone license file for `nvpermissive-core.o`.

Therefore / Поэтому:

- do not assume the core is MIT / нельзя автоматически считать core MIT-компонентом;
- the wrapper's MIT license does not override core terms;
- preserve this notice if an EFI binary embeds the core;
- safest public distribution is source-only or requiring users to provide the core separately until redistribution rights are confirmed.

---

## 4. Public CMP 90HX Windows unlock research / Публичные исследования CMP 90HX Windows unlock

### WildFlash1st/cmp90hx-unlock-for-windows

https://github.com/WildFlash1st/cmp90hx-unlock-for-windows

Used as a public research/reference source for CMP 90HX pre-OS unlock architecture, SEC2/Falcon flow, PLM concepts, Windows handoff, no-POST requirements, reset behavior, and multi-card research.

The repository credits bendy2, Jon Pry, the cmpunlocker community, and d3dx9.

A specific license for the exact commit used as reference was not independently verified in this review. Treat as reference-only unless the exact `LICENSE` at the copied commit is checked.

---

## 5. CMP unlock community sources / Источники сообщества CMP unlock

### amoghmunikote/cmpunlocker
https://github.com/amoghmunikote/cmpunlocker  
**License / Лицензия:** GPL-2.0

### WebForks/cmpunlocker
https://github.com/WebForks/cmpunlocker  
**License / Лицензия:** GPL-2.0-only

### xrip/cmp50hx-unlock
https://github.com/xrip/cmp50hx-unlock

Related issue / Связанная issue:
https://github.com/xrip/cmp50hx-unlock/issues/8

Some content is GPL-derived, so exact file/commit licensing must be checked before reuse.

### bendy2/cmp90hx
https://github.com/bendy2/cmp90hx

V67 exploit and persistent CMP 90HX compute-unlock research. License not independently established during this review.

### d3dx9/cmpunlocker
https://github.com/d3dx9/cmpunlocker

Falcon BootROM, firmware-signature, PLM, emulator, and ROP-chain research. License not independently established during this review.

---

## 6. Academic research source / Научный источник

Jon Pry — **A Canary in the Crypto Mine: Defeating Stack Protection in a GPU Secure Coprocessor**

https://doi.org/10.5281/zenodo.20916112

This is a research citation, not a software license. / Это исследовательская ссылка, а не лицензия на программный код.

---

## 7. NVIDIA sources and binaries / Источники и бинарные файлы NVIDIA

### NVIDIA Open GPU Kernel Modules
https://github.com/NVIDIA/open-gpu-kernel-modules

Preserve exact per-file SPDX/license notices.

### NVIDIA proprietary drivers / firmware / GSP blobs

Examples: Windows display drivers, Linux `.run` packages, `gsp_ga10x.bin`, signed Falcon/SEC2/GSP firmware.

Do not assume these are MIT/GPL just because open-source tools use them. Prefer download/extraction instructions instead of redistributing proprietary binaries.

---

## 8. Microsoft Windows Boot Manager / Загрузчик Windows

Path: `\EFI\Microsoft\Boot\bootmgfw.efi`

The project may chainload the user's existing Windows Boot Manager. Do not redistribute `bootmgfw.efi` in GitHub releases.

---

## 9. UEFI / EDK II references / UEFI / EDK II

TianoCore EDK II: https://github.com/tianocore/edk2  
License: https://github.com/tianocore/edk2/blob/master/License.txt  
**Primary license / Основная лицензия:** BSD-2-Clause-Patent

---

# PART B / ЧАСТЬ B — BeeLlama / llama.cpp CMP 90HX inference stack

## 10. Rhonstin/beellama-cmp90hx

https://github.com/Rhonstin/beellama-cmp90hx  
**License / Лицензия:** MIT

CMP 90HX / GA102 / SM 8.6 optimizations, including DP4A → IMAD and FP32 dequantization → HFMA2 paths.

Preserve the MIT license, upstream llama.cpp notices, and bundled `licenses/` directory.

## 11. llama.cpp

https://github.com/ggml-org/llama.cpp  
License: https://github.com/ggml-org/llama.cpp/blob/master/LICENSE  
**License / Лицензия:** MIT

## 12. BeeLlama third-party dependencies / Сторонние зависимости BeeLlama

| Component / Компонент | Role / Назначение | License / Лицензия |
|---|---|---|
| `cpp-httplib` | HTTP server/client | MIT |
| `stb-image` | image decoder | Public Domain |
| `nlohmann/json` | JSON library | MIT |
| `miniaudio.h` | audio decoder | Public Domain |
| `subprocess.h` | process helper | Public Domain |
| Snowflake ArcticInference components | suffix-tree / int32-map | Apache-2.0 |
| Intel OpenVINO frontend header | OpenVINO backend | Apache-2.0 |
| Intel SYCL/oneAPI backend | SYCL backend | Apache-2.0 WITH LLVM-exception |

For full texts, preserve BeeLlama's upstream `licenses/` directory.

## 13. BeeLlama feature-origin references / Источники отдельных функций BeeLlama

- `TheTom/llama-cpp-turboquant`
- `spiritbuun/buun-llama-cpp`

Before directly copying code, check and preserve the exact license of the commit used.

---

# PART C / ЧАСТЬ C — Optional graphics driver patching / Опциональный патчинг графического драйвера

## 14. dartraiden/NVIDIA-patcher

https://github.com/dartraiden/NVIDIA-patcher

Optional reference for graphics/3D acceleration patching on mining cards including CMP 90HX. A clear top-level open-source license was not independently verified during this review.

Do not redistribute NVIDIA proprietary driver binaries under this project's license.

---

# PART D / ЧАСТЬ D — Recommended repository layout / Рекомендуемая структура репозитория

```text
/
├─ LICENSE
├─ SOURCES_AND_LICENSES_RU_EN.md
├─ THIRD_PARTY_NOTICES.md
├─ README.md
├─ src/
├─ include/
└─ tools/
```

Suggested source header / Рекомендуемый заголовок:

```c
// SPDX-License-Identifier: MIT
//
// Derived from the MIT-licensed NVPermissiveEFI Windows wrapper.
// Third-party components retain their own licenses.
// See SOURCES_AND_LICENSES_RU_EN.md and THIRD_PARTY_NOTICES.md.
```

---

# PART E / ЧАСТЬ E — Redistribution checklist / Чек-лист перед публикацией

- [ ] Keep the wrapper's MIT `LICENSE`.
- [ ] Keep `SOURCES_AND_LICENSES_RU_EN.md`.
- [ ] Keep a third-party notice for `nvpermissive-core.o`.
- [ ] Do not label the whole EFI binary as MIT unless core terms are resolved.
- [ ] Prefer source-only distribution if core redistribution is uncertain.
- [ ] Do not commit proprietary NVIDIA firmware/driver blobs without permission.
- [ ] Do not commit Microsoft's `bootmgfw.efi`.
- [ ] Preserve GPL obligations for copied GPL-derived code.
- [ ] Preserve EDK II notices if EDK II source is copied.
- [ ] Preserve BeeLlama/llama.cpp MIT notices and `licenses/`.
- [ ] Record exact upstream commits/tags.
- [ ] Record hashes for external core/firmware blobs.
- [ ] Separate code actually used from research/reference sources.

---

## Disclaimer / Отказ от ответственности

This file is a technical provenance and license-summary document, not legal advice.

Этот файл является техническим описанием происхождения кода и сводкой лицензий, а не юридической консультацией.
