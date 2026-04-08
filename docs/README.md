# Система высокоскоростного обмена данными на базе ПЛИС с использованием интерфейса USB 3.0

## Обзор

Проект реализует тракт обмена данными на базе `Xilinx Spartan-6` и `FTDI FT601` в режиме `245 synchronous FIFO`.

Основной рабочий сценарий:

- принять 8-битный поток данных по GPIO;
- упаковать байты в 32-битные слова;
- буферизовать поток в FIFO;
- передать данные на ПК через `USB 3.0`.

Дополнительно предусмотрен тестовый loopback-режим для проверки FT601 без подачи данных на GPIO.

## Потоки данных

Основной режим:

`GPIO -> get_gpio -> packer8to32 -> fifo_tx -> fifo_fsm -> FT601 -> ПК`

Тестовый loopback-режим:

`ПК -> FT601 -> fifo_rx -> fifo_fsm -> FT601 -> ПК`

## Верхний модуль

Текущий верхний модуль:

- `fpga/top.v`

Внешние интерфейсы:

- GPIO:
  - `GPIO_CLK`
  - `GPIO_DATA[7:0]`
  - `GPIO_STROB`
  - `FPGA_RESET`
- FT601:
  - `CLK`
  - `RESET_N`
  - `TXE_N`
  - `RXF_N`
  - `OE_N`
  - `WR_N`
  - `RD_N`
  - `BE[3:0]`
  - `DATA[31:0]`

## Тактовые домены

Используются два домена.

### GPIO-домен

- источник такта: `GPIO_CLK`
- целевая частота: до `50 МГц`
- основные блоки:
  - `get_gpio.v`
  - `packer8to32.v`
  - write-side `fifo_dualport.v`
  - write-side `sram_dualport.v`
  - `fifo_tx_ctrl.v`

### FT601-домен

- источник такта: `CLK` от FT601
- текущая целевая частота: `66.67 МГц`
- основные блоки:
  - read-side `fifo_dualport.v`
  - read-side `sram_dualport.v`
  - `fifo_fsm.v`
  - `fifo_rx_ctrl.v`
  - `ft601_io.v`

## Сброс

Внутренняя логика использует synchronized active-low reset по доменам.

Внешние источники reset:

- `FPGA_RESET`
- `RESET_N`

Внутренние сигналы:

- `gpio_rst_n_i`
- `ft_rst_n_i`

Синхронизация reset выполняется в `fpga/rst_sync.v`.

## Основные модули

### `fpga/top.v`

Верхний модуль, который соединяет два домена, FT601 I/O wrapper, FIFO, SRAM и управляющую логику.

### `fpga/ft601_io.v`

Физическая обвязка FT601.

Использует:

- `IBUFG` для `CLK`
- `IBUF` для `RESET_N`, `TXE_N`, `RXF_N`
- `OBUF` для `OE_N`, `WR_N`, `RD_N`
- `IOBUF` для `DATA[31:0]` и `BE[3:0]`

### `fpga/get_gpio.v`

Захват входного GPIO-потока и формирование локальных сигналов `data/strobe/clk` для write-domain.

### `fpga/packer8to32.v`

Упаковка четырех валидных байтов в одно 32-битное слово.

### `fpga/fifo_dualport.v`

Асинхронный FIFO с раздельными тактами записи и чтения, бинарными и Gray-указателями, sticky `overflow` и опциональным `underflow`.

### `fpga/sram_dualport.v`

Двухпортовая синхронная память для FIFO.

### `fpga/fifo_tx_ctrl.v`

Управление записью в TX FIFO и блокировка новых слов при sticky ошибках.

### `fpga/fifo_rx_ctrl.v`

Обработка слов, полученных от FT601, в normal mode.

В normal mode RX тракт зарезервирован под команды от хоста.

Поддерживаемые команды при `BE = 4'hF`:

- `32'h00000001` - очистить TX error
- `32'h00000002` - очистить RX error
- `32'h00000003` - очистить обе ошибки

В loopback mode этот блок не владеет RX FIFO.

### `fpga/fifo_fsm.v`

Конечный автомат FT601 интерфейса.

Функции:

- арбитраж между TX и RX;
- генерация `WR_N`, `RD_N`, `OE_N`;
- управление `fifo_pop` и `fifo_append`;
- управление TX burst и RX burst.

Текущие состояния:

- `ARB`
- `TX_PREFETCH`
- `TX_BURST`
- `RX_START`
- `RX_BURST`

## Режимы работы

### Normal mode

Управляется параметром:

- `FT_LOOPBACK_TEST = 1'b0`

Поведение:

- полезные данные приходят с GPIO;
- FT601 передает их на ПК;
- RX канал FT601 используется только для служебных команд.

Произвольный payload с ПК в этом режиме не считается штатным сценарием.

### FT loopback mode

Управляется параметром:

- `FT_LOOPBACK_TEST = 1'b1`

Поведение:

- ПК записывает произвольные данные в FT601;
- FPGA принимает их в RX FIFO;
- затем эти же данные возвращаются обратно через FT601 TX path.

В этом режиме входные данные от ПК трактуются как payload, а не как команды.

## Обработка ошибок

Используются sticky ошибки.

### TX error

Фиксируется при ошибках TX-side, включая попытку записи в полный TX FIFO.

### RX error

Фиксируется при ошибках RX-side, включая overflow RX FIFO.

Очистка ошибок выполняется командами от хоста в normal mode.

## Верификация

Основной testbench:

- `fpga/testbench.v`

Что проверяется:

- reset в обоих доменах;
- соответствие режима `FT_LOOPBACK_TEST`;
- normal mode `GPIO -> FT601`;
- loopback mode `FT601 -> FPGA -> FT601`;
- корректность данных на RX и TX;
- фазировка сигналов FT601:
  - `OE_N` активируется на такт раньше `RD_N`;
  - RX завершение проверяется по фазе после `RXF_N`;
  - TX завершение проверяется по фазе после `TXE_N`;
- поведение при временном backpressure от FT601.

Loopback stimulus и GPIO stimulus формируются из `fpga/data_p`.

## Constraints и отчеты

Основные файлы:

- `fpga/callistoS6.ucf`
- `fpga/top.twr`
- `fpga/top.twx`
- `fpga/top.par`
- `fpga/top.pad`

`UCF` задает:

- pinout для Callisto S6;
- timing constraints для FT601;
- ограничения верхнего уровня по тактам и интерфейсам.

## Состав репозитория

Основные файлы в `fpga/`:

- `top.v`
- `ft601_io.v`
- `rst_sync.v`
- `get_gpio.v`
- `packer8to32.v`
- `fifo_dualport.v`
- `sram_dualport.v`
- `fifo_fsm.v`
- `fifo_tx_ctrl.v`
- `fifo_rx_ctrl.v`
- `testbench.v`
- `callistoS6.ucf`

Материалы в `docs/`:

- документация FT601;
- application note FTDI по master FIFO;
- reference design FTDI для Spartan-6.

## Текущее ограничение

Основная незавершенная инженерная задача сейчас одна:

- уменьшить стартовую latency на TX-path.

Функционально проект работает, но старт TX пока медленнее, чем в reference design FTDI.
