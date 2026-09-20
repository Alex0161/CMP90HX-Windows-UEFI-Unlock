CMP90HX MANUAL-4 ALL-ORIGINAL-V2

Ручной режим: каждая карта запускается отдельно, без автоматического перехода
к следующей карте и без автоматического запуска Windows.

ВАЖНО:
  GPU1, GPU2, GPU3 и GPU4 теперь ОБРАБАТЫВАЮТСЯ АБСОЛЮТНО ОДИНАКОВО.

Для всех четырех:
  - один и тот же original v2 core;
  - обычный CPUCTL (без CPUCTL_ALIAS);
  - одинаковый initial dual-SBR;
  - compute;
  - одинаковый post-compute dual-SBR;
  - graphics;
  - одинаковый post-graphics dual-SBR;
  - windows-boundary verification.

Никаких специальных исключений для GPU4 больше нет.

Меню:
  1  Unlock GPU1 only
  2  Unlock GPU2 only
  3  Unlock GPU3 only
  4  Unlock GPU4 only — ТОЧНО ТАК ЖЕ, КАК GPU1-3
  C  Check SS0/SS1/GFX всех четырех
  W  Вручную запустить Windows (только если реальный check = 4/4 PASS)
  H  Halt

Рекомендуемый порядок:
  1 -> дождаться PASS
  2 -> дождаться PASS
  3 -> дождаться PASS
  4 -> дождаться PASS
  C -> 4/4 PASS
  W -> Windows

Между 1/2/3/4 НЕ ДЕЛАТЬ reboot/reset/power-cycle.
Если карта FAIL, можно снова нажать её номер в этом же UEFI-сеансе.

Установка:
  заменить на FAT32 флешке:
      EFI\BOOT\BOOTX64.EFI

Перед первым запуском:
  полностью выключить питание на 10-15 секунд.