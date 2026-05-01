# SPECIFICATION

## Назначение

Проект описывает текущую RTL-архитектуру для `Xilinx Spartan-6` и `FTDI FT601` в режиме `245 synchronous FIFO`. Один bitstream поддерживает три рабочих сценария: передача GPIO-потока в ПК, возврат данных в loopback-режиме и служебный control/status обмен с хоста.

После полного сброса система стартует в `normal mode`. Хост может переключать режимы и читать статус через framed service protocol без перепрошивки FPGA.

## Внешние интерфейсы

Со стороны GPIO используются `GPIO_CLK`, `GPIO_DATA[7:0]`, `GPIO_STROB` и `FPGA_RESET`. `GPIO_CLK` задает write-домен. Байты на `GPIO_DATA[7:0]` захватываются по `GPIO_STROB`, после чего `packer8to32` собирает четыре байта в одно 32-битное слово для normal TX FIFO.

`FPGA_RESET` - внешний active-high reset request. Он является главным сбросом проекта и влияет на оба внутренних домена, а также на физический `RESET_N`, который FPGA выдает в FT601.

Со стороны FT601 используются сигналы synchronous FIFO bus: `CLK`, `TXE_N`, `RXF_N`, `OE_N`, `WR_N`, `RD_N`, `RESET_N`, `DATA[31:0]` и `BE[3:0]`. `CLK` формирует FT-домен. `TXE_N=0` означает, что FT601 готов принять слово от FPGA. `RXF_N=0` означает, что FT601 имеет слово для FPGA. `WR_N=0` записывает данные в FT601. `OE_N=0` вместе с `RD_N=0` разрешает чтение слова из FT601.

`DATA[31:0]` и `BE[3:0]` являются двунаправленными шинами. Во время TX FPGA драйвит обе шины через `ft601_wrapper.v`; во время RX и reset они переводятся в tri-state.

## Система сброса

Reset в проекте разделен по назначению. Есть физический hard reset через `FPGA_RESET`, есть локальный FT-state clear по service-команде, есть recovery clear для ошибок, есть TX prefetch flush при переключении источников. Эти действия не смешиваются в одну линию сброса.

`FPGA_RESET` проходит через `IBUF` в `top.v` и становится внутренним `fpga_reset_i`. Из него формируются два reset request:

```verilog
gpio_rst_req = fpga_reset_i;
ft_rst_req   = fpga_reset_i;
```

Для каждого домена используется `rst_sync.v`. Он асинхронно активирует reset и синхронно отпускает его в своем домене. В GPIO-домене результатом является `gpio_rst_n_i`, в FT-домене - `ft_rst_n_i`.

Физический reset FT601 формируется напрямую от внешнего reset request:

```verilog
ft601_reset_n_i = ~fpga_reset_i;
```

Этот сигнал приходит в `ft601_wrapper.v` и через `OBUF` выходит на pin `RESET_N`. Когда `FPGA_RESET=1`, FPGA держит `RESET_N=0`, `WR_N/RD_N/OE_N` находятся в неактивном состоянии, а `DATA/BE` не драйвятся. После отпускания `FPGA_RESET` FT601 получает `RESET_N=1`, а логика FPGA выходит из reset синхронно с соответствующими clock-доменами.

В GPIO-домене reset очищает `gpio_wrapper`, `packer8to32`, `tx_write_guard` и write-side `async_fifo`. В FT-домене reset очищает `ft601_fsm`, RX/TX adapters, `rx_stream_router`, `service_cmd_decoder`, `status_source`, `loopback_fifo` и read-side `async_fifo`. После reset `service_cmd_decoder` устанавливает `loopback_mode=0`, поэтому система находится в `normal mode`.

Команда `CMD_RESET_FT_STATE` работает мягче. Она очищает локальное FT-domain state, RX/status/TX adapter state и recovery logic, переводит режим в normal и сохраняет содержимое normal TX async FIFO. Физический `RESET_N` FT601 при этом остается под управлением `FPGA_RESET`.

Команды `CMD_CLR_TX_ERROR`, `CMD_CLR_RX_ERROR` и `CMD_CLR_ALL_ERROR` обслуживают recovery path. Они очищают диагностическое состояние и соответствующие recovery-clear импульсы, но не выполняют полный reset домена.

## Основная архитектура

Верхний уровень находится в `source/top.v`. Он соединяет GPIO-домен, FT-домен, FIFO, stream-router, arbiter, command decoder и status source. Последовательная логика распределена по небольшим модулям, поэтому `top.v` остается схемой соединений.

Физическая граница FT601 находится в `ft601_wrapper.v`. Там стоят буферы `IBUFG`, `IBUF`, `OBUF`, `IOBUF`, входная регистрация `TXE_N/RXF_N`, output-регистры управляющих сигналов и регистры для `DATA/BE`. Wrapper принимает уже готовые внутренние сигналы от FSM/adapters и выдает безопасный внешний интерфейс FT601.

`ft601_fsm.v` задает фазы доступа к FT601. RX-захват вынесен в `ft601_rx_adapter.v`; TX output/prefetch path вынесен в `ft601_tx_adapter.v`. Благодаря этому FSM координирует доступ к шине, а datapath хранится в отдельных блоках.

Внутренние потоки связаны AXI-Stream-подобным контрактом: `valid`, `ready`, `data`, `keep`. Передача слова происходит при `valid && ready`. Источник держит `data/keep` стабильными, пока приемник не подтвердил передачу через `ready`. `keep[3:0]` соответствует `BE[3:0]`, а `data[31:0]` соответствует `DATA[31:0]`. Синхронные FIFO read ports обернуты в `sync_fifo_axis_source.v`, чтобы arbiter работал со стабильным stream, а не с raw `ren/empty/data`.

Основные stream-ветки:

| Поток | Назначение |
| --- | --- |
| `normal_axis_*` | payload из normal TX FIFO |
| `loopback_axis_*` | payload из loopback FIFO |
| `status_axis_*` | status response source |
| `tx_axis_*` | общий TX stream после arbitration |
| `ft_rx_axis_*` | поток слов, принятых от FT601 |

`axis_tx_arbiter.v` выбирает источник для TX path. Приоритет фиксированный: status response, затем loopback при `loopback_mode=1`, затем normal TX FIFO при `loopback_mode=0`. Status frame удерживает источник до отправки двух слов: `STATUS_MAGIC` и `status_word`.

`rx_stream_router.v` принимает слова от FT601 RX adapter и разделяет их на service traffic и loopback payload. Service frame потребляется внутри control path. Payload-слова в loopback mode записываются в `loopback_fifo` как `{BE, DATA}`.

## Datapath режимов

В `normal mode` данные идут от GPIO к ПК:

```text
GPIO -> gpio_wrapper -> packer8to32 -> async_fifo -> sync_fifo_axis_source -> axis_tx_arbiter -> ft601_tx_adapter -> ft601_wrapper -> FT601 -> PC
```

GPIO-домен пишет слова в normal TX FIFO. FT-домен читает их через `sync_fifo_axis_source`, который держит front/lookahead слова и выдает `normal_axis_*`. FT601 RX path остается активным для service-команд.

В `FT loopback mode` данные приходят с ПК и возвращаются обратно:

```text
PC -> FT601 -> ft601_rx_adapter -> rx_stream_router -> loopback_fifo -> sync_fifo_axis_source -> axis_tx_arbiter -> ft601_tx_adapter -> FT601 -> PC
```

Loopback FIFO хранит 36 бит на слово: `{BE[3:0], DATA[31:0]}`. Это сохраняет byte-enable информацию. Командные слова service frame в эту FIFO не записываются. Read-side loopback FIFO также проходит через `sync_fifo_axis_source`, поэтому loopback TX path соблюдает тот же `valid/ready/data/keep` contract.

Status path работает как отдельный TX-source. По `CMD_GET_STATUS` блок `status_source.v` формирует двухсловный response. Если TX burst уже активен, status ждет безопасное окно; если payload только ожидает отправки, status получает приоритет.

## Framed service protocol

Service traffic идет по тем же endpoints, что и payload: команды пишутся в `EP02`, ответы читаются из `EP82`. Для service/status слов используется полный 32-битный beat с `BE=4'hF`.

Команда состоит из двух 32-битных слов:

```text
CMD_MAGIC = 32'hA55A5AA5
opcode
```

Parser распознает команду после полного слова `CMD_MAGIC`. Следующее полное слово потребляется как opcode. Известный opcode запускает действие. Неизвестный opcode завершает frame без изменения состояния. Оба слова service frame остаются внутри control path.

Поддерживаемые opcode:

| Opcode | Значение | Действие |
| --- | --- | --- |
| `CMD_CLR_TX_ERROR` | `32'h00000001` | очистить TX diagnostic state |
| `CMD_CLR_RX_ERROR` | `32'h00000002` | очистить RX diagnostic state |
| `CMD_CLR_ALL_ERROR` | `32'h00000003` | очистить TX и RX errors |
| `CMD_SET_LOOPBACK` | `32'hA5A50004` | перейти в loopback mode |
| `CMD_SET_NORMAL` | `32'hA5A50005` | вернуться в normal mode |
| `CMD_GET_STATUS` | `32'hA5A50006` | запросить status response |
| `CMD_RESET_FT_STATE` | `32'hA5A50007` | очистить локальное FT state с сохранением normal TX FIFO |

`CMD_RESET_FT_STATE` реализован в RTL как служебная команда низкого уровня. В текущем `ft601_test` отдельного пункта меню для нее нет: ручная проверка строится вокруг `GET_STATUS`, `SET_LOOPBACK`, `SET_NORMAL`, loopback integrity и `CMD_CLR_*`.

Ответ на `CMD_GET_STATUS`:

```text
STATUS_MAGIC = 32'h5AA55AA5
status_word
```

`status_word`:

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

Переходы между `normal` и `loopback` выполняет `service_cmd_decoder.v`. При смене режима он выставляет `service_hold`, дожидается idle-состояния FT path, формирует локальный clear для FT state, flush-ит TX prefetch path и только потом фиксирует новый `loopback_mode`.

`service_hold` блокирует новые операции на время controlled switch. `tx_flush` очищает pending word внутри TX adapter. Recovery clear очищает диагностическое состояние и выдает clear-импульсы в нужный FIFO path.

## Диагностика

Внешний статус содержит агрегированные `tx_error` и `rx_error`. Эти флаги формируются из событий подсистемы:

| Агрегат | События |
| --- | --- |
| `tx_error` | запрос записи в full normal TX FIFO; запрос чтения из empty normal TX FIFO |
| `rx_error` | запрос записи в full loopback FIFO; запрос чтения из empty loopback FIFO |

Ошибки удерживаются до service-команды очистки. `CMD_CLR_TX_ERROR` очищает TX-состояние, `CMD_CLR_RX_ERROR` очищает RX-состояние, `CMD_CLR_ALL_ERROR` очищает оба направления. Текущие значения попадают в `status_word[1]` и `status_word[2]`.

## FT601 handshake и timing

На внешней границе FT601 используются зарегистрированные управляющие сигналы. `TXE_N` и `RXF_N` принимаются через `ft601_wrapper.v`, регистрируются в FT-домене и затем используются FSM/adapters. `WR_N`, `RD_N`, `OE_N`, `DATA`, `BE` также проходят через зарегистрированную boundary-логику wrapper.

TX path работает от общего `tx_axis_*` stream. Когда выбранный источник имеет данные, `ft601_tx_adapter.v` подготавливает слово, управляет prefetch/output-регистрами, выставляет `DATA/BE` и активирует `WR_N` только в write-фазе. `drive_tx` управляет tri-state отдельно от `WR_N`, поэтому шина данных включается и выключается явно.

RX path работает через `ft601_rx_adapter.v`. FSM активирует read-фазы, adapter управляет `OE_N/RD_N`, сэмплирует `DATA/BE` и выдает слово в `ft_rx_axis_*`. Backpressure приходит через `ft_rx_axis_tready`: в loopback mode он зависит от свободного места в `loopback_fifo`, в normal mode RX path готов принимать service traffic.

Основные требования к handshake: отсутствует прямой combinational path от `TXE_N/RXF_N` pad к `WR_N/RD_N/OE_N`; `WR_N` и `drive_tx` остаются независимыми; при backpressure слова сохраняют порядок; во время active write `DATA/BE` стабильны; во время RX и reset FPGA не драйвит FT601 data bus.

## Проверка

Основной самопроверочный стенд - `source/testbench.v`. Он проверяет reset, старт в `normal mode`, GPIO-to-FT601 path, runtime-вход в loopback, возврат в normal, status frame, recovery commands, backpressure и отделение service traffic от payload.

### Сценарии использования

Testbench построен вокруг сценариев, которые соответствуют реальным действиям с платой и `ft601_test`.

Сначала проверяется базовый запуск. Пользователь включает плату или нажимает `FPGA_RESET`, после чего дизайн должен выйти в безопасное состояние, отпустить `RESET_N` для FT601 и стартовать в `normal mode`.

Дальше хост запрашивает статус FPGA. Это базовая проверка связи PC -> FT601 -> FPGA -> FT601 -> PC: команда `GET_STATUS` должна вернуть статусный frame, где видно, что loopback выключен, ошибки не выставлены, а FIFO находятся в ожидаемом состоянии.

В `normal mode` проверяется основной поток данных от GPIO к ПК. Дизайн принимает байты со стороны GPIO, собирает их в 32-битные слова, кладет в normal TX FIFO и отдает их в FT601 без перестановок и потери `BE`.

Затем пользователь включает loopback командой `SET_LOOPBACK` и подтверждает режим через `GET_STATUS`. После этого можно отправить payload с ПК и прочитать его обратно. Принятые слова должны совпасть с отправленными слово в слово.

После проверки loopback пользователь возвращает дизайн в `normal mode` командой `SET_NORMAL` и снова подтверждает состояние через `GET_STATUS`. Дальнейшая передача должна снова идти из normal GPIO path, а не из loopback FIFO.

Отдельный сценарий проверяет loopback после сброса. Сначала loopback включается и проходит короткий payload compare, затем подается `FPGA_RESET`, после release режим должен вернуться в `normal`. После повторного `SET_LOOPBACK` loopback payload compare должен снова пройти.

Диагностика проверяется через команды очистки. Testbench создает диагностическое состояние, затем выполняет `CMD_CLR_TX_ERROR`, `CMD_CLR_RX_ERROR` и `CMD_CLR_ALL_ERROR`. Результат каждый раз проверяется не чтением внутренних регистров, а обычным `GET_STATUS`.

Последний базовый сценарий - backpressure. Он моделирует ситуации, когда FT601 временно не готов принимать данные или не имеет данных для чтения. Дизайн не должен писать при `TXE_N=1`, не должен читать при `RXF_N=1`, не должен портить порядок payload и не должен создавать конфликт на `DATA/BE`.

### Техническое покрытие сценариев

| Сценарий | Что делает testbench | Критерий прохождения |
| --- | --- | --- |
| `reset_boot_normal` | Активирует `FPGA_RESET`, проверяет inactive `WR_N/RD_N/OE_N`, tri-state `DATA/BE`, release доменных reset и `RESET_N`. | После release дизайн в `normal mode`, FT601 bus безопасен. |
| `get_status_after_reset` | Отправляет `CMD_MAGIC + CMD_GET_STATUS` по RX path и открывает TX path для ответа. | На TX выходит `STATUS_MAGIC + status_word`, `loopback_mode=0`, ошибок нет. |
| `normal_payload_integrity` | Подает байтовый поток в GPIO/packer path, держит FT601 TX закрытым, затем разрешает передачу. | FT601 TX получает ожидаемые 32-битные слова в том же порядке, `BE=4'hF`, RX path не активируется. |
| `set_loopback_and_status` | Отправляет `CMD_SET_LOOPBACK`, затем `CMD_GET_STATUS`. | Status подтверждает `loopback_mode=1`. |
| `loopback_payload_integrity` | В loopback mode подает payload через FT601 RX и затем разрешает FT601 TX. | Количество, порядок, `DATA` и `BE` на TX совпадают с отправленным RX payload. |
| `set_normal_and_status` | Отправляет `CMD_SET_NORMAL`, затем `CMD_GET_STATUS`, после этого повторяет normal payload check. | Status подтверждает `loopback_mode=0`, TX source снова normal path. |
| `loopback_after_reset` | Выполняет короткий loopback compare, подает `FPGA_RESET`, повторно включает loopback и снова сравнивает payload. | Reset очищает режим, повторный loopback работает без ручной правки внутренних состояний. |
| `diagnostic_clear` | Инжектирует диагностические TX/RX error-состояния, отправляет `CMD_CLR_TX_ERROR`, `CMD_CLR_RX_ERROR`, `CMD_CLR_ALL_ERROR`. | После каждой команды `GET_STATUS` показывает ожидаемые `tx_error/rx_error`. |
| `ft_backpressure` | Держит `TXE_N=1` при pending TX payload, затем разрешает TX; отдельно держит `RXF_N=1`. | Нет записи при закрытом TX, нет чтения при пустом RX, payload не продвигается во время backpressure. |

Обязательные локальные команды:

```powershell
cd C:\Users\userIvan\Desktop\my_projects\logic_analyzer\source
iverilog -g2005-sv -o testbench.out testbench.v
vvp .\testbench.out
$env:VERILATOR_ROOT='C:\msys64\mingw64\share\verilator'
verilator_bin.exe --lint-only --timing testbench.v
```

После timing-sensitive RTL-изменений используется ISE flow. Минимум - `xst -ifn top.xst -ofn top.syr`. Для оценки частоты нужны post-PAR отчеты `top.twr` и `top.twx`.

Host-side проверка выполняется через `ft601_test`. Базовый сценарий: `GET_STATUS`, `SET_LOOPBACK`, `Loopback integrity test`, `SET_NORMAL`, `CLR_ALL_ERROR`, снова `GET_STATUS`. При исправном loopback отправленный payload совпадает с принятым слово в слово.

## Исходники текущей архитектуры

Ключевые RTL-файлы: `top.v`, `ft601_wrapper.v`, `ft601_fsm.v`, `ft601_rx_adapter.v`, `ft601_tx_adapter.v`, `sync_fifo_axis_source.v`, `axis_tx_arbiter.v`, `rx_stream_router.v`, `service_cmd_decoder.v`, `status_source.v`, `async_fifo.v`, `loopback_fifo.v`, `sram_dualport.v`, `tx_write_guard.v`, `gpio_wrapper.v`, `packer8to32.v`, `rst_sync.v`, `bit_sync.v`, `pulse_sync.v`.

Ограничения лежат в `source/callistoS6.ucf`. Testbench - `source/testbench.v`. Host-side проверка - в `ft601_test/`.
