# Система обмена данными FPGA - FT601 - PC

Этот проект реализует высокоскоростной тракт обмена данными между `Xilinx Spartan-6` и ПК через мост `FTDI FT601` в режиме `245 synchronous FIFO`.

Основной поток простой: байты приходят по GPIO, упаковываются в 32-битные слова, проходят через FIFO и уходят в ПК через USB 3.0. В том же bitstream есть loopback-режим. Он нужен для проверки FT601 и host-side софта без внешнего GPIO-источника: ПК отправляет данные в FPGA, а FPGA возвращает их обратно через тот же FT601.

Проект сейчас ориентирован на один универсальный bitstream. После reset система стартует в `normal mode`, а режимы переключаются служебными командами с ПК.

## Как это работает

В `normal mode` полезные данные идут по цепочке:

```text
GPIO -> gpio_wrapper -> packer8to32 -> async_fifo -> axis_tx_arbiter -> ft601_tx_adapter -> ft601_wrapper -> FT601 -> PC
```

GPIO-домен и FT601-домен разделены асинхронным FIFO. Это штатный режим для будущего захвата данных: GPIO остается источником, FT601 RX path используется только для служебных команд.

В `loopback mode` источник данных меняется:

```text
PC -> FT601 -> ft601_rx_adapter -> rx_stream_router -> loopback_fifo -> axis_tx_arbiter -> ft601_tx_adapter -> FT601 -> PC
```

Loopback FIFO хранит не только `DATA[31:0]`, но и `BE[3:0]`, поэтому payload возвращается с тем же byte-enable mask. Service frame при этом не попадает в payload.

Третий поток - status response. Он формируется внутри FPGA по команде `CMD_GET_STATUS` и имеет приоритет над normal/loopback payload. Это важно: `STATUS_MAGIC` и `status_word` должны выйти подряд, без вклинивания пользовательских данных.

## Service protocol

Служебный протокол идет поверх обычных 32-битных слов FT601. Отдельный control endpoint не используется.

Команда состоит из двух слов по `EP02`:

```text
CMD_MAGIC = 0xA55A5AA5
opcode
```

Ответ на `CMD_GET_STATUS` тоже состоит из двух слов, но читается из `EP82`:

```text
STATUS_MAGIC = 0x5AA55AA5
status_word
```

Для service/status слов ожидается полный beat, то есть `BE = 4'hF`. `BE` больше не является признаком команды.

Поддерживаемые команды:

| Opcode | Значение | Назначение |
| --- | --- | --- |
| `CMD_CLR_TX_ERROR` | `0x00000001` | очистить TX error |
| `CMD_CLR_RX_ERROR` | `0x00000002` | очистить RX error |
| `CMD_CLR_ALL_ERROR` | `0x00000003` | очистить оба error-флага |
| `CMD_SET_LOOPBACK` | `0xA5A50004` | перейти в loopback mode |
| `CMD_SET_NORMAL` | `0xA5A50005` | вернуться в normal mode |
| `CMD_GET_STATUS` | `0xA5A50006` | прочитать status frame |
| `CMD_RESET_FT_STATE` | `0xA5A50007` | очистить локальное FT-domain state без очистки normal TX FIFO |

`status_word` пока компактный. В нем есть только текущий режим, агрегированные ошибки и empty/full флаги двух FIFO:

| Бит | Поле |
| --- | --- |
| `0` | `loopback_mode` |
| `1` | `tx_error` |
| `2` | `rx_error` |
| `3` | `tx_fifo_empty` |
| `4` | `tx_fifo_full` |
| `5` | `loopback_fifo_empty` |
| `6` | `loopback_fifo_full` |
| `31:7` | `0` |

## Reset model

`FPGA_RESET` - главный внешний hard reset проекта. Он сбрасывает оба домена FPGA и одновременно управляет выходом `RESET_N` в FT601. То есть `RESET_N` для FT601 генерируется внутри FPGA, а не приходит снаружи как входной сигнал.

После reset дизайн возвращается в `normal mode`. Управляющие сигналы FT601 неактивны, `DATA/BE` находятся в tri-state. Host-команды не дергают физический `RESET_N`; они могут очищать только локальное FT-domain state или recovery logic. Важное ограничение: host-side FT-state clear не должен очищать normal TX async FIFO.

## Структура репозитория

`source/` содержит RTL, testbench и constraints. Главная точка входа - `source/top.v`. Физическая граница FT601 находится в `ft601_wrapper.v`, а read/write handshake разбит между `ft601_fsm.v`, `ft601_rx_adapter.v` и `ft601_tx_adapter.v`. Выбор TX-источника делает `axis_tx_arbiter.v`, RX service/payload path разделяет `rx_stream_router.v`, а команды разбирает `service_cmd_decoder.v`.

`SPECIFICATION.md` содержит актуальное техническое описание протокола, reset-системы, FT601 handshake, datapath и проверок.

`ft601_test/` - консольная C++ утилита для ручной проверки с ПК через D3XX API. Код разделен по смыслу: `main.cpp` содержит меню, `ft601_device.*` работает с D3XX и pipe, `service_protocol.*` отправляет команды и читает статус, `payload_test.*` генерирует payload и делает loopback compare.

## Проверка RTL

Базовая проверка выполняется из `source/`:

```powershell
cd C:\Users\userIvan\Desktop\my_projects\logic_analyzer\source
iverilog -g2005-sv -o testbench.out testbench.v
vvp .\testbench.out
$env:VERILATOR_ROOT='C:\msys64\mingw64\share\verilator'
verilator_bin.exe --lint-only --timing testbench.v
```

Для timing-sensitive изменений дополнительно нужен ISE flow и просмотр `top.syr`, `top.twr`, `top.twx`. Сейчас фиксированная latency из FTDI master FIFO reference не считается требованием проекта. Требование другое: не должно быть прямого combinational path от `TXE_N/RXF_N` pad к `WR_N/RD_N/OE_N`; handshake должен быть корректным, а `DATA/BE` должны стабильно вести себя во время write, read и reset.

## Проверка с ПК

Сборка `ft601_test` описана в `ft601_test/README.md`. Текущая команда использует `MSYS2 MinGW x64` и библиотеку D3XX из `WU_FTD3XXLib`.

Минимальный ручной сценарий на железе такой. Сначала прошить FPGA и убедиться, что FT601 настроен в `245 synchronous FIFO mode`. Затем запустить `ft601_test`, прочитать `Get FPGA status`, включить `Set loopback mode` и выполнить `Loopback integrity test`. После этого можно вернуться через `Set normal mode` и при необходимости очистить ошибки командами `Clear TX error`, `Clear RX error` или `Clear all errors`.

Raw-операции `Write test payload` и `Read payload to file` остаются как debug-инструменты. Они не используются для чтения status frame.

## Куда смотреть дальше

Для быстрого входа достаточно трех файлов: `source/top.v`, `SPECIFICATION.md` и `ft601_test/README.md`. Если нужно отлаживать FT601 handshake, рядом почти всегда понадобятся `ft601_wrapper.v`, `ft601_fsm.v`, `ft601_rx_adapter.v`, `ft601_tx_adapter.v` и waveform из `source/testbench.vcd`.