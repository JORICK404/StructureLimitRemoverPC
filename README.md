# StructureLimitRemover (LeviLamina)

Мод для [LeviLamina](https://github.com/LiteLDev/LeviLamina): порт
[StructureLimitRemover](https://github.com/fangkuaichaoge/StructureLimitRemover) (Android) на
**Minecraft Bedrock для Windows**.

Снимает лимит 64 блока по X и Z для структур:

- поля размера в интерфейсе структурного блока (больше не обрезаются до 64);
- серверная обработка данных структурного блока;
- кнопка «Экспорт» в структурном блоке (проверка шаблона перед сохранением `.mcstructure`);
- `/structure save`;
- скриптовый `world.structureManager` (варианты с `BlockVolume` и `Vector3`).

Высота (Y) по-прежнему ограничена высотой измерения, как и в ванилле.

## Установка

1. Поставь клиентскую LeviLamina, например через [LeviLauncher](https://github.com/LiteLDev/LeviLauncher).
2. Скопируй папку `bin/StructureLimitRemover/` (в ней `StructureLimitRemover.dll` и `manifest.json`)
   в `mods/` рядом с игрой. Должно получиться `mods/StructureLimitRemover/manifest.json`.
3. Запусти игру. В логе LeviLamina появится строка
   `Structure size limit removed (N site(s) patched, ...)`.

Выключение мода через LeviLamina (`disable`) возвращает исходные байты, и лимит снова начинает работать.

## Сборка

Нужны [xmake](https://xmake.io), Visual Studio 2022 и LLVM (clang-cl) в `PATH`.

```bash
xmake f -a x64 -m release -p windows --target_type=client -y
```

```bash
xmake -y
```

Готовый мод появится в `bin/StructureLimitRemover/`. Чтобы собрать под сервер (BDS), используй
`--target_type=server`. Сигнатуры проверялись только на клиенте, поэтому на сервере часть мест
может быть не найдена (это будет видно в логе).

## Как это работает

На Android лимит задаётся в одной маленькой функции-хелпере. В Windows-сборке её встроили
(inline) во все места вызова, поэтому патчится каждое место отдельно. Все сравнения в x64 там
используют 8-битную константу (`cmp r32, 0x40`), а в неё 999 не помещается. Поэтому мод
не поднимает лимит до 999, а выключает проверки:

| Место | Было | Стало |
|---|---|---|
| `/structure` (X, Z) | `cmp ecx,40h / ja tooBig` | `nop` |
| скрипты, `BlockVolume` | `cmp r,3Fh / jg err` | `nop` |
| скрипты, `Vector3` | `ucomiss x,64.0 / jbe ok` | `jmp ok` |
| экспорт (X, Z) | `cmp [r13+30h],40h / jg fail` | `nop` |
| кламп в UI и на сервере, X | `cmp r,40h / cmovge r,64` | `cmovo` (никогда не срабатывает) |
| кламп в UI и на сервере, Z | `cmp r,40h / cmovl c,r` | `cmovno` (срабатывает всегда) |

Каждое место ищется по сигнатуре в `.text` игры. Перед записью мод проверяет исходные байты,
поэтому на незнакомой версии игры он ничего не сломает: такие места просто попадут в лог как
`not found`.

Проверено на Minecraft **1.26.51.01** (LeviLamina 26.51) и **1.26.52**: на обеих версиях
находятся все 10 мест.

## Осторожно

- Верхнего предела по X/Z больше нет. Если сохранить структуру размером в тысячи блоков, игра
  может надолго зависнуть или упасть из-за нехватки памяти.
- После обновления игры сигнатуры могут перестать совпадать. Это будет видно в логе.
