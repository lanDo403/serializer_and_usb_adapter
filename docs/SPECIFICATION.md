# SPECIFICATION

## Назначение

Проект описывает текущую RTL-архитектуру для `Xilinx Spartan-6` и `FTDI FT601` в режиме `245 synchronous FIFO`. Один bitstream поддерживает три host-visible сценария: передача GPIO-потока в ПК, возврат данных в loopback-режиме и служебный status/control обмен.

После полного reset система всегда стартует в `normal mode`. Дальше ПК может переключить режим через framed service protocol без перепрошивки FPGA.

## Внешние интерфейсы

Со стороны GPIO используются `GPIO_CLK`, `GPIO_DATA[7:0]`, `GPIO_STROB` и `FPGA_RESET`. `GPIO_CLK` задает write-домен. Данные считаются валидными по `GPIO_STROB`, затем `packer8to32` собирает четыре байта в одно 32-битное слово. `FPGA_RESET` является общим внешним hard reset для проекта.

Со стороны FT601 используются стандартные сигналы synchronous FIFO bus: `CLK`, `TXE_N`, `RXF_N`, `OE_N`, `WR_N`, `RD_N`, `RESET_N`, `DATA[31:0]` и `BE[3:0]`. `CLK` формирует FT-домен. `TXE_N=0` означает, что FT601 готов принять слово от FPGA. `RXF_N=0` означает, что FT601 имеет слово для FPGA. `WR_N=0` записывает данные в FT601, а `OE_N=0` вместе с `RD_N=0` используется для чтения.

`RESET_N` - выход FPGA в FT601. Он active-low и управляется от `FPGA_RESET`.

## Тактовые домены и reset

В проекте два рабочих домена. GPIO-домен работает от `GPIO_CLK` и содержит `gpio_wrapper`, `packer8to32`, write-side `async_fifo` и `tx_write_guard`. FT-домен работает от `CLK` FT601 и содержит FT601 boundary, FSM, adapters, RX router, command decoder, status source, loopback FIFO и read-side `async_fifo`.

Reset устроен просто. `FPGA_RESET` приходит в `top.v`, буферизуется и используется как reset request для обоих доменов. `rst_sync` делает asynchronous assert и synchronous release отдельно для `gpio_clk` и `ft_clk_i`. Пока reset активен, FPGA держит `RESET_N=0` для FT601, управляющие сигналы `WR_N/RD_N/OE_N` неактивны, а `DATA/BE` находятся в tri-state.

После reset `service_cmd_decoder` держит `loopback_mode=0`, FIFO находятся в empty-state, status response не pending. Host-команда `CMD_RESET_FT_STATE` очищает локальное FT-domain state и recovery logic, но не является физическим reset FT601 и не очищает normal TX async FIFO.

## Основная архитектура

Верхний уровень находится в `source/top.v`. Он не содержит сложной последовательной логики, а соединяет домены, FIFO, FT601 adapters, stream arbiter, router и command/status блоки.

FT601 boundary разделен на несколько блоков. `ft601_wrapper.v` содержит физические буферы, регистрирует `TXE_N/RXF_N`, управляет IOB-регистрами и tri-state для `DATA/BE`. `ft601_fsm.v` координирует фазы доступа к FT601, но не держит весь datapath внутри одного большого блока. RX-сэмплинг вынесен в `ft601_rx_adapter.v`, TX prefetch/output pipeline - в `ft601_tx_adapter.v`.

Для внутренней связи используется AXI-Stream-подобный стиль: `valid`, `ready`, `data`, `keep`. Это не внешний AXI IP и не SystemVerilog `interface`, а локальный контракт между модулями. Передача слова происходит при `valid && ready`. Пока `valid=1` и handshake еще не произошел, источник должен держать `data/keep` стабильными. `tlast` намеренно не вводится: границы service/status frame уже заданы магическими словами протокола.

Основные stream-ветки:

| Поток | Назначение |
| --- | --- |
| `normal_axis_*` | payload из normal TX FIFO |
| `loopback_axis_*` | payload из loopback FIFO |
| `status_axis_*` | status response source |
| `tx_axis_*` | общий TX stream после arbitration |
| `ft_rx_axis_*` | поток слов, принятых от FT601 |

`axis_tx_arbiter.v` выбирает один TX-источник с приоритетом `status -> loopback -> normal`. Status frame должен выйти атомарно: `STATUS_MAGIC`, затем `status_word`, без payload между ними.

`rx_stream_router.v` разделяет RX-слова от FT601 на service traffic и loopback payload. Он потребляет service frame, не пропускает его в loopback FIFO и при этом сохраняет одиночные слова без `CMD_MAGIC` как обычный payload. Это важно для сценария, где payload случайно совпал с opcode, но не был предварен magic-word.

## Datapath режимов

В `normal mode` данные идут от GPIO к ПК:

```text
GPIO -> gpio_wrapper -> packer8to32 -> async_fifo -> axis_tx_arbiter -> ft601_tx_adapter -> ft601_wrapper -> FT601 -> PC
```

FT601 RX path в этом режиме остается активным для service-команд. Если включается loopback, запись новых GPIO-слов в normal TX FIFO блокируется через `tx_write_guard`.

В `FT loopback mode` данные приходят с ПК и возвращаются обратно:

```text
PC -> FT601 -> ft601_rx_adapter -> rx_stream_router -> loopback_fifo -> axis_tx_arbiter -> ft601_tx_adapter -> FT601 -> PC
```

Loopback FIFO хранит 36 бит на слово: `{BE[3:0], DATA[31:0]}`. Это сохраняет byte-enable информацию и не превращает неполные слова в обычный full-word payload.

Status path работает параллельно payload path, но имеет приоритет на TX. По `CMD_GET_STATUS` блок `status_source.v` формирует двухсловный response. Если FT601 TX burst уже идет, status ждет следующего безопасного idle-window. Если payload только pending, status выходит первым.

## Framed service protocol

Service traffic идет по тем же endpoints, что и payload: команды по `EP02`, ответы по `EP82`. Команды больше не маркируются через особое значение `BE`. Для service/status слов ожидается полный beat, то есть `BE=4'hF`.

Команда состоит из двух 32-битных слов:

```text
CMD_MAGIC = 32'hA55A5AA5
opcode
```

Parser распознает команду только после полного слова `CMD_MAGIC`. Следующее полное слово потребляется как opcode. Если opcode неизвестен, frame отбрасывается без побочных эффектов и оба слова не попадают в loopback payload.

Поддерживаемые opcode:

| Opcode | Значение | Действие |
| --- | --- | --- |
| `CMD_CLR_TX_ERROR` | `32'h00000001` | очистить TX diagnostic sticky state |
| `CMD_CLR_RX_ERROR` | `32'h00000002` | очистить RX diagnostic sticky state |
| `CMD_CLR_ALL_ERROR` | `32'h00000003` | очистить TX и RX errors |
| `CMD_SET_LOOPBACK` | `32'hA5A50004` | перейти в loopback mode |
| `CMD_SET_NORMAL` | `32'hA5A50005` | вернуться в normal mode |
| `CMD_GET_STATUS` | `32'hA5A50006` | запросить status response |
| `CMD_RESET_FT_STATE` | `32'hA5A50007` | очистить локальное FT state без очистки normal TX FIFO |

Ответ на `CMD_GET_STATUS`:

```text
STATUS_MAGIC = 32'h5AA55AA5
status_word
```

`status_word` остается совместимым с текущей host-утилитой:

| Биты | Значение |
| --- | --- |
| `0` | `loopback_mode` |
| `1` | `tx_error` |
| `2` | `rx_error` |
| `3` | `tx_fifo_empty` |
| `4` | `tx_fifo_full` |
| `5` | `loopback_fifo_empty` |
| `6` | `loopback_fifo_full` |
| `31:7` | `0` |

## Mode switch и recovery

Переходы между `normal` и `loopback` выполняются только через command decoder. Декодер не меняет режим мгновенно в середине активного TX. Он выставляет `service_hold`, дожидается безопасного idle состояния, очищает локальные FT/RX/TX adapter state, при необходимости flush-ит TX prefetch и только потом фиксирует новый режим.

`service_hold` не является reset. Он просто блокирует новые операции на время controlled switch. `tx_flush` очищает pending TX adapter word. Recovery clear очищает диагностическое состояние и соответствующие FIFO/recovery paths. Эти операции разведены по смыслу, чтобы не маскировать ошибки и не очищать лишние данные.

## Диагностика

Внешний статус пока содержит только агрегированные `tx_error` и `rx_error`. Внутри они строятся не на декоративных FIFO sticky flags, а на реальных событиях подсистем:

| Агрегат | События |
| --- | --- |
| `tx_error` | write request в full normal TX FIFO, read request из empty normal TX FIFO |
| `rx_error` | write request в full loopback FIFO, read request из empty loopback FIFO |

Команды `CMD_CLR_TX_ERROR`, `CMD_CLR_RX_ERROR` и `CMD_CLR_ALL_ERROR` очищают эти sticky-состояния. Формат `status_word` от этого не расширяется.

## FT601 handshake и timing

Текущая архитектура не обязана повторять latency из FTDI master FIFO reference. Значения `3/4/1` были характеристикой reference-дизайна, а не жестким требованием этого проекта.

Обязательные требования другие. Нельзя строить combinational direct path от `TXE_N/RXF_N` pad к `WR_N/RD_N/OE_N`. `WR_N` и `drive_tx` должны оставаться независимыми сигналами. При TX backpressure нельзя терять, дублировать или переставлять слова. Во время active write шины `DATA/BE` должны быть стабильны, а во время RX и reset FPGA не должна драйвить FT601 data bus.

Желательно сохранять хотя бы один FT clock latency между изменением `TXE_N/RXF_N` и активностью соответствующих control-сигналов. Но главным критерием остается корректный handshake, отсутствие underflow/overflow в штатных сценариях и прохождение timing constraints.

## Проверка

Основной самопроверочный стенд - `source/testbench.v`. Он должен подтверждать reset, `normal mode`, runtime-вход в loopback, возврат в normal, status frame, recovery commands, backpressure и отсутствие смешивания service traffic с payload.

Обязательные локальные команды:

```powershell
cd C:\Users\userIvan\Desktop\my_projects\logic_analyzer\source
iverilog -g2005-sv -o testbench.out testbench.v
vvp .\testbench.out
$env:VERILATOR_ROOT='C:\msys64\mingw64\share\verilator'
verilator_bin.exe --lint-only --timing testbench.v
```

После timing-sensitive RTL-изменений нужен ISE flow. Минимум - `xst -ifn top.xst -ofn top.syr`. Для окончательного решения по частоте смотреть post-PAR отчеты `top.twr` и `top.twx`, а не только synthesis estimate.

Host-side проверка выполняется через `ft601_test`. Базовый сценарий: `GET_STATUS`, `SET_LOOPBACK`, `Loopback integrity test`, `SET_NORMAL`, `CLR_ALL_ERROR`, снова `GET_STATUS`. Если loopback исправен, отправленный payload должен совпасть с принятым слово в слово.

## Исходники, которые входят в текущую архитектуру

Ключевые RTL-файлы: `top.v`, `ft601_wrapper.v`, `ft601_fsm.v`, `ft601_rx_adapter.v`, `ft601_tx_adapter.v`, `axis_tx_arbiter.v`, `rx_stream_router.v`, `service_cmd_decoder.v`, `status_source.v`, `async_fifo.v`, `loopback_fifo.v`, `sram_dualport.v`, `tx_write_guard.v`, `gpio_wrapper.v`, `packer8to32.v`, `rst_sync.v`, `bit_sync.v`, `pulse_sync.v`.

Ограничения лежат в `source/callistoS6.ucf`. Testbench - `source/testbench.v`. Host-side проверка - в `ft601_test/`.