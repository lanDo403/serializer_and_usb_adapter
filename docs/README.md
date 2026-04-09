# Система высокоскоростного обмена данными на базе ПЛИС с использованием интерфейса USB 3.0

## Обзор

Проект реализует тракт обмена данными на базе `Xilinx Spartan-6` и `FTDI FT601` в режиме `245 synchronous FIFO`.

Основной рабочий сценарий:

- принять 8-битный поток по GPIO;
- упаковать данные в 32-битные слова;
- буферизовать поток во внутреннем FIFO;
- передать данные на ПК через `USB 3.0`.

Дополнительно есть loopback-режим для проверки FT601 без источника данных на GPIO.

## Потоки данных

### Основной режим

`GPIO -> get_gpio -> packer8to32 -> fifo_tx -> fifo_fsm -> FT601 -> PC`

### Loopback-режим

`PC -> FT601 -> fifo_fsm -> fifo_rx -> fifo_fsm -> FT601 -> PC`

## Верхний уровень

Основной верхний модуль:

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
  - `fpga/get_gpio.v`
  - `fpga/packer8to32.v`
  - write-side `fpga/fifo_dualport.v`
  - write-side `fpga/sram_dualport.v`
  - `fpga/fifo_tx_ctrl.v`

### FT601-домен

- источник такта: `CLK` от FT601
- текущая рабочая частота: `66.67 МГц`
- основные блоки:
  - read-side `fpga/fifo_dualport.v`
  - read-side `fpga/sram_dualport.v`
  - `fpga/fifo_fsm.v`
  - `fpga/fifo_rx_ctrl.v`
  - `fpga/ft601_io.v`

## Сброс

Внутренняя логика использует synchronized active-low reset по доменам.

Внешние сигналы сброса:

- `FPGA_RESET`
- `RESET_N`

Внутренние reset-сигналы:

- `gpio_rst_n_i`
- `ft_rst_n_i`

Синхронизация выполняется в `fpga/rst_sync.v`.

## Основные модули

### `fpga/top.v`

Связывает оба домена, FT601 I/O wrapper, FIFO, SRAM и управляющую логику.

### `fpga/ft601_io.v`

Физическая обвязка FT601:

- `IBUFG` для `CLK`
- `IBUF` для `RESET_N`, `TXE_N`, `RXF_N`
- `OBUF` для `OE_N`, `WR_N`, `RD_N`
- `IOBUF` для `DATA[31:0]` и `BE[3:0]`

### `fpga/get_gpio.v`

Захват входного GPIO-потока и формирование локальных сигналов для write-domain.

### `fpga/packer8to32.v`

Упаковка четырех байтов в одно 32-битное слово.

### `fpga/fifo_dualport.v`

Асинхронный FIFO с отдельными тактами записи и чтения, sticky `overflow` и опциональным `underflow`.

### `fpga/sram_dualport.v`

Двухпортовая память для FIFO.

### `fpga/fifo_tx_ctrl.v`

Управление записью в TX FIFO и блокировка новых слов при sticky TX error.

### `fpga/fifo_rx_ctrl.v`

Обработка слов, пришедших с FT601, в normal mode.

В normal mode RX тракт используется только для команд от ПК. При `BE = 4'hF` поддерживаются:

- `32'h00000001` — очистить TX error
- `32'h00000002` — очистить RX error
- `32'h00000003` — очистить обе ошибки

В loopback-режиме этот блок не владеет `fifo_rx`.

### `fpga/fifo_fsm.v`

FSM FT601-интерфейса. Отвечает за:

- арбитраж между RX и TX;
- генерацию `WR_N`, `RD_N`, `OE_N`;
- `fifo_pop` и `fifo_append`;
- RX и TX burst.

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

Произвольный payload от ПК в этом режиме не считается штатным сценарием.

### FT loopback mode

Управляется параметром:

- `FT_LOOPBACK_TEST = 1'b1`

Поведение:

- ПК записывает произвольные данные в FT601;
- FPGA принимает их во внутренний RX FIFO;
- затем эти же данные отправляются обратно через FT601 TX path.

В этом режиме входной поток от ПК трактуется как payload, а не как команды.

## Обработка ошибок

Используются sticky ошибки.

### TX error

Фиксируется при ошибках TX-side, включая попытку записи в полный TX FIFO.

### RX error

Фиксируется при ошибках RX-side, включая overflow RX FIFO.

Очистка выполняется командами от хоста в normal mode.

## Верификация

Основной testbench:

- `fpga/testbench.v`

Что сейчас проверяется:

- reset в обоих доменах;
- normal mode;
- loopback mode;
- корректность RX и TX данных;
- фазировка FT601 сигналов:
  - `OE_N` активируется раньше `RD_N`;
  - RX завершение проверяется относительно `RXF_N`;
  - TX завершение проверяется относительно `TXE_N`;
- поведение при backpressure от FT601;
- TX latency:
  - `TXE_N active -> WR_N active = 3 FT clocks`
  - `TXE_N inactive -> WR_N inactive = 1 FT clock`

Stimulus для GPIO и loopback собирается из `fpga/data_p`.

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
- FTDI soft
- полезные материалы по асинхронному FIFO и временному анализу;

## Текущее ограничение

Сейчас `FT_LOOPBACK_TEST` используется как compile-time параметр. Это удобно для локальной проверки, но в таком виде у решения есть архитектурный минус:

- при `FT_LOOPBACK_TEST = 1'b1` synthesis вырезает normal-path;
- при `FT_LOOPBACK_TEST = 1'b0` loopback не является runtime-режимом;
- часть warning на synthesis/translate/map связана именно с этим;
- фактически проект пока живет как две разные конфигурации одного дизайна.

Это будет исправлено переходом на универсальный bitstream с runtime-переключением режима.
