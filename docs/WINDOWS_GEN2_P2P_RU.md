# Эксперимент: CMP 90HX PCIe Gen2 + P2P под Windows

Эта ветка не заменяет проверенный \`v0.1.0\`. Она создана отдельно для переноса идей из Linux-проектов на Windows.

## Что перенесено

### PCIe Gen2 до запуска Windows

В UEFI-меню добавлена команда:

\`\`\`text
G  EXPERIMENTAL: PCIe Gen2 pass on all four unlocked GPUs
\`\`\`

Порядок теста:

\`\`\`text
1 -> 2 -> 3 -> 4 -> C -> G -> C -> W
\`\`\`

\`G\` запускается только после фактического \`4/4 PASS\`.

Алгоритм retrain изменён по результатам Linux-тестов: вместо одного Retrain-Link используется до **5 раундов**, в каждом раунде RL выставляется повторно и состояние линии проверяется до **4 секунд**.

### Windows P2P registry experiment

\`tools/windows-p2p-enable.ps1\` находит **только CMP 90HX (10DE:220D)** в display-class registry и устанавливает экспериментальные RM regkeys:

\`\`\`text
ForceP2P                    = 0x111
CLForceP2P                  = 0x111
RMForceP2PType              = 1
RMPcieP2PType               = 0
RMForceStaticBar1           = 1
PeerMappingOverride         = 1
RMDisableFeatureDisablement = 1
\`\`\`

Перед изменением значения сохраняются в:

\`\`\`text
tools/cmp90hx-p2p-registry-backup.json
\`\`\`

Откат:

\`\`\`powershell
PowerShell -ExecutionPolicy Bypass -File .\tools\windows-p2p-restore.ps1
\`\`\`

## Почему это эксперимент

Исследование Windows registry показывает, что драйвер NVIDIA действительно читает такие ключи из display-adapter class key. Но это **не доказывает**, что Windows 591.86 разрешит CUDA P2P на CMP 90HX.

Linux-реализация дополнительно:

- переводит IOMMU groups в \`identity\`;
- отключает ACS redirects;
- перезагружает patched \`nvidia.ko\`.

Прямого безопасного userspace-эквивалента этих шагов под Windows здесь нет. Поэтому первый Windows-прототип ограничен UEFI Gen2 + RM registry keys без патча \`nvlddmkm.sys\`.

## Применение registry

PowerShell от администратора:

\`\`\`powershell
Set-ExecutionPolicy -Scope Process Bypass
.\tools\windows-p2p-enable.ps1 -WhatIfOnly
.\tools\windows-p2p-enable.ps1
\`\`\`

Затем полное выключение/включение и UEFI:

\`\`\`text
1 -> 2 -> 3 -> 4 -> C -> G -> C -> W
\`\`\`

## Проверка Windows

\`\`\`powershell
nvidia-smi
nvidia-smi topo -m
nvidia-smi topo -p2p r
nvidia-smi topo -p2p w
\`\`\`

Для фактической CUDA-проверки:

\`\`\`powershell
nvcc -O2 -arch=sm_86 .\tools\cuda_p2p_test.cu -o .\cuda_p2p_test.exe
.\cuda_p2p_test.exe
\`\`\`

Тест выводит:

- \`cudaDeviceCanAccessPeer\` для каждой пары CMP;
- реальный \`cudaMemcpyPeer\`;
- GB/s между каждой парой карт.

## Критерий успеха

\`\`\`text
GPUx -> GPUy: YES
GPUx -> GPUy: >0 GB/s
\`\`\`

Если \`cudaDeviceCanAccessPeer == 0\`, одних registry key недостаточно.

## Откат

\`\`\`powershell
.\tools\windows-p2p-restore.ps1
\`\`\`

После отката используйте проверенный путь без \`G\`:

\`\`\`text
1 -> 2 -> 3 -> 4 -> C -> W
\`\`\`

Не публикуйте эту ветку как стабильный release до аппаратной проверки.
