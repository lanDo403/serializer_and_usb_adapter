# SPECIFICATION

## 1. Назначение проекта

Проект реализует систему высокоскоростного обмена данными на базе ПЛИС `Xilinx Spartan-6` с использованием моста `FTDI FT601` в режиме `245 synchronous FIFO`.

Один bitstream поддерживает два режима работы:

1. `Normal mode` — передача данных с GPIO в ПК через FT601.
2. `FT loopback mode` — прием данных от ПК через FT601 и возврат этих же данных обратно в FT601 без участия GPIO.

## 2. Верхнеуровневые интерфейсы

### 2.1. GPIO-интерфейс

Входные сигналы:

- `GPIO_CLK`
- `GPIO_DATA[7:0]`
- `GPIO_STROB`
- `FPGA_RESET`

Назначение:

- `GPIO_CLK` задает write-домен.
- `GPIO_DATA[7:0]` содержит входные байты.
- `GPIO_STROB` отмечает валидность входного байта.
- `FPGA_RESET` сбрасывает логику FPGA и возвращает систему в `normal mode`.

### 2.2. FT601-интерфейс

Сигналы:

- входы от FT601:
  - `CLK`
  - `RESET_N`
  - `TXE_N`
  - `RXF_N`
- выходы в FT601:
  - `OE_N`
  - `WR_N`
  - `RD_N`
- двунаправленные шины:
  - `BE[3:0]`
  - `DATA[31:0]`

Назначение:

- `CLK` задает FT-домен.
- `TXE_N` сообщает, что FT601 готов принимать данные от FPGA.
- `RXF_N` сообщает, что FT601 содержит данные для FPGA.
- `OE_N` и `RD_N` используются для чтения из FT601.
- `WR_N` используется для записи в FT601.
- `BE[3:0]` и `DATA[31:0]` образуют рабочую шину обмена.

## 3. Архитектура и datapath

### 3.1. Тактовые домены

Система разделена на два домена.

#### GPIO domain

Источник такта: `GPIO_CLK`

Функции:

- захват входного GPIO-потока;
- упаковка байтов в 32-битные слова;
- запись слов в TX FIFO.

#### FT domain

Источник такта: `CLK` от FT601

Функции:

- FT601 handshake;
- чтение из FT601;
- запись в FT601;
- потоковая обработка команд;
- loopback.

### 3.2. Основные блоки

#### `fpga/top.v`

Связывает оба домена, FIFO, FT601 wrapper, FSM и управляющие блоки.

#### `fpga/ft601_io.v`

Физическая обвязка FT601:

- `IBUFG` для `CLK`;
- `IBUF` для `RESET_N`, `TXE_N`, `RXF_N`;
- `OBUF` для `OE_N`, `WR_N`, `RD_N`;
- `IOBUF` для `DATA[31:0]` и `BE[3:0]`.

Особенность текущей реализации:

- `TXE_N` и `RXF_N` после `IBUF` захватываются явными регистрами в домене `clk_o`;
- наружу в остальной дизайн выходят только зарегистрированные `txe_n_o` и `rxf_n_o`;
- эти регистры являются целевой точкой завершения timing-анализа по `OFFSET IN` для control-флагов FT601.

#### `fpga/bit_sync.v`

Локальный single-bit CDC helper.

Текущее назначение:

- синхронизация `loopback_mode` из FT-домена в `gpio_clk`;
- сохранение `top.v` как чистого интеграционного модуля без собственной последовательностной CDC-логики.

#### `fpga/get_gpio.v`

Захват GPIO-сигналов и формирование локальных write-domain сигналов.

#### `fpga/packer8to32.v`

Упаковка четырех валидных байтов в одно 32-битное слово.

#### `fpga/fifo_dualport.v` + `fpga/sram_dualport.v`

Асинхронный TX FIFO между GPIO domain и FT domain.

Назначение:

- буферизация основного потока `GPIO -> FT601`;
- раздельные такты записи и чтения;
- фиксация `overflow` и `underflow`.

#### `fpga/fifo_singleclock.v`

Single-clock FIFO в домене `ft_clk_i` для loopback.

Формат хранения:

- ширина слова `36` бит;
- формат `{BE, DATA}`.

Loopback FIFO не хранит только `DATA[31:0]`, так как `BE` должен сохраняться вместе с payload.

#### `fpga/loopback_ft_ctrl.v`

FT-domain блок локального runtime-управления loopback-путем.

Текущее назначение:

- регистрация принятых RX-слов `{BE, DATA}`;
- отделение командного слова от loopback payload при входе в loopback;
- формирование записи в `loopback_fifo`;
- генерация `tx_prefetch_en` и `tx_source_change` для `fifo_fsm`.

#### `fpga/host_cmd_ctrl.v`

Потоковый декодер слов, пришедших с FT601.

Текущее назначение:

- обработка служебных RX-слов в `normal mode`;
- фиксация и очистка sticky-ошибок;
- включение runtime loopback режима.

RX-команды больше не обязаны проходить через отдельный async RX FIFO: они обрабатываются потоково по принятым словам в FT-домене.

#### `fpga/status_ft_ctrl.v`

FT-domain источник статусного ответного слова.

Текущее назначение:

- формирование одного TX control beat по `CMD_GET_STATUS`;
- выдача `status_word` через FT601 TX path;
- запуск ответа только в безопасном окне, когда внутренний TX pipeline свободен.

#### `fpga/tx_write_guard.v`

Управление записью в TX FIFO со стороны GPIO domain.

Функции:

- разрешение записи packed-слов в TX FIFO;
- блокировка приема новых GPIO-слов после sticky-ошибки;
- очистка TX error по команде от хоста.

#### `fpga/fifo_fsm.v`

FSM FT601-интерфейса.

Функции:

- арбитраж между RX и TX;
- генерация `WR_N`, `RD_N`, `OE_N`;
- управление `drive_tx`;
- формирование `fifo_pop` и `fifo_append`;
- burst-чтение и burst-запись.

Текущие состояния:

- `ARB`
- `TX_PREFETCH`
- `TX_BURST`
- `RX_START`
- `RX_BURST`

### 3.3. Текущий рабочий datapath

#### Normal mode

`GPIO -> get_gpio -> packer8to32 -> fifo_tx -> fifo_fsm -> FT601 -> PC`

Особенности:

- полезные данные приходят с GPIO;
- FT601 RX path используется только для служебных команд;
- запись из GPIO в TX FIFO блокируется при активном loopback режиме.

#### FT loopback mode

`PC -> FT601 -> fifo_fsm -> loopback_ft_ctrl -> loopback_fifo -> fifo_fsm -> FT601 -> PC`

Особенности:

- payload принимается через обычный FT601 RX handshake;
- принятое слово сразу фиксируется как `{BE, DATA}`;
- слово пишется в `loopback_fifo`;
- затем тот же payload возвращается в FT601 TX path;
- GPIO путь на время loopback не используется как источник данных для FT601 TX.

## 4. Режимы работы

### 4.1. Normal mode

Это режим по умолчанию после reset.

Поведение:

1. Данные приходят с GPIO.
2. Каждые 4 валидных байта упаковываются в 32-битное слово.
3. Слова записываются в TX FIFO.
4. При активном `TXE_N = 0` FT601 принимает эти слова от FPGA.
5. RX-слова, пришедшие с FT601, трактуются как командный поток.

### 4.2. FT loopback mode

Loopback включается во время работы без перепрошивки.

Способ входа:

- команда `32'hA5A50004`, принятая через FT601 RX path;
- команда передается как `control beat` с `BE = 4'hE`.

Поведение:

1. ПК отправляет 32-битные слова в FT601.
2. FPGA принимает их через FT601 RX handshake.
3. Слова записываются в `loopback_fifo` как `{BE, DATA}`.
4. Далее те же слова выдаются обратно через FT601 TX path.
5. Обычные RX-слова рассматриваются как payload, а `control beat` продолжает трактоваться как служебное слово и не попадает в payload.

### 4.3. Выход из loopback mode

Штатный выход выполняется командой `32'hA5A50005`, принятой через FT601 RX path как `control beat`.

После этого система обязана:

1. запретить старт новых локальных операций;
2. дождаться безопасного idle-окна FT-domain;
3. локально очистить FIFO и внутренний state;
4. вернуться в `normal mode`;
5. снова разрешить путь `GPIO -> TX FIFO -> FT601`.

`FPGA_RESET` остается полным глобальным reset и тоже возвращает систему в `normal mode`.

## 5. Командный протокол

Командный поток идет через `control beat` и должен распознаваться в обоих режимах.

Слово считается командой, если:

1. `BE = 4'hE`;
2. `DATA[31:0]` совпадает с одним из поддерживаемых кодов.

### 5.1. Поддерживаемые команды

- `32'h00000001` — очистить TX error;
- `32'h00000002` — очистить RX error;
- `32'h00000003` — очистить обе ошибки;
- `32'hA5A50004` — включить loopback mode.
- `32'hA5A50005` — вернуть `normal mode`.
- `32'hA5A50006` — запросить статусный ответ.

### 5.2. Ограничения по loopback-команде

1. Команда включения loopback должна быть принята через обычный FT601 RX handshake.
2. Командное слово не должно попасть в loopback payload.
3. После входа в loopback обычные RX-слова трактуются как payload, а `control beat` остается служебным словом.

### 5.3. Статусный ответ

На `CMD_GET_STATUS` FPGA должна вернуть один TX control beat:

- `BE = 4'hE`
- `DATA = status_word`

Формат `status_word`:

- `bit[0]` — `loopback_mode`
- `bit[1]` — `tx_error`
- `bit[2]` — `rx_error`
- `bit[3]` — `tx_fifo_empty`
- `bit[4]` — `tx_fifo_full`
- `bit[5]` — `loopback_fifo_empty`
- `bit[6]` — `loopback_fifo_full`
- `bit[31:7]` — `0`

## 6. Сброс

### 6.1. Внешние сигналы сброса

- `FPGA_RESET` — внешний сброс со стороны платы;
- `RESET_N` — active-low reset от FT601.

### 6.2. Внутренние требования

1. Внутренняя логика использует synchronized active-low reset по доменам.
2. После `FPGA_RESET` система обязана вернуться в `normal mode`.
3. Во время reset управляющие выходы FT601 должны быть неактивны:
   - `WR_N = 1`
   - `RD_N = 1`
   - `OE_N = 1`
4. Во время reset FPGA не должна драйвить FT601 data bus.

## 7. Timing и handshake требования

### 7.1. FT601 control-флаги

Внутри дизайна должны использоваться только зарегистрированные версии `TXE_N` и `RXF_N`.

Требования:

- сырые сигналы после `IBUF` не должны использоваться вне `ft601_io`;
- timing-анализ control-флагов должен завершаться на входных регистрах FT-домена;
- `WR_N` и `drive_tx` должны быть независимыми сигналами;
- `drive_tx` не должен быть простой инверсией `WR_N`.

### 7.2. RX временная диаграмма

Целевая фазировка FT601 RX path:

- `RXF_N active -> OE_N active = 3 FT clocks`;
- `RXF_N active -> RD_N active = 4 FT clocks`;
- `RXF_N inactive -> OE_N inactive = 1 FT clock`;
- `RXF_N inactive -> RD_N inactive = 1 FT clock`.

Дополнительные требования:

- `OE_N` должен активироваться раньше `RD_N`;
- после снятия `RXF_N` сигналы `OE_N` и `RD_N` должны отпускаться с корректной фазировкой;
- RX payload должен приниматься без потери первого слова и без лишнего append в конце burst.

### 7.3. TX временная диаграмма

Целевая фазировка FT601 TX path:

- `TXE_N active -> WR_N active = 3 FT clocks`;
- `TXE_N inactive -> WR_N inactive = 1 FT clock`.

Дополнительные требования:

- внутри burst `WR_N` не должен пульсировать на каждом слове;
- пока `TXE_N` остается активным и данные доступны, burst должен идти непрерывно;
- при backpressure со стороны FT601 burst должен корректно останавливаться и продолжаться без потери порядка слов;
- TX datapath не должен повторно выдавать предыдущее слово при обновлении burst.

## 8. Ошибки и защита

Используются sticky-ошибки.

### 8.1. TX error

Фиксируется при:

- попытке записи в полный TX FIFO;
- TX-side underflow, если такой сценарий зарегистрирован логикой FIFO.

### 8.2. RX error

Фиксируется при:

- overflow loopback FIFO;
- underflow loopback FIFO;
- других RX-side ошибках, если они зарегистрированы логикой FIFO.

### 8.3. Очистка ошибок

Ошибки очищаются командами от хоста через FT601 RX path в виде `control beat`.

## 9. Verification requirements

Основной проверочный стенд: `fpga/testbench.v`.

Testbench должен покрывать:

1. reset sequence;
2. старт в `normal mode` после reset;
3. normal mode: `GPIO -> FT601`;
4. подтверждение, что TX не стартует при `TXE_N = 1`;
5. backpressure на TX path;
6. команду входа в loopback;
7. loopback mode: `FT601 -> FPGA -> FT601`;
8. возврат в `normal mode` по `CMD_SET_NORMAL`;
9. возврат в `normal mode` по `FPGA_RESET`;
10. `CMD_GET_STATUS` в `normal mode`;
11. `CMD_GET_STATUS` в `loopback mode`;
12. `CMD_GET_STATUS` после recovery-команд;
13. корректность RX и TX данных;
14. фазировку FT601 RX/TX handshake;
15. количественное измерение RX и TX latency.

Stimulus для GPIO и loopback берется из `fpga/data_p`.

## 10. Файлы реализации

Основные HDL-файлы:

- `fpga/top.v`
- `fpga/ft601_io.v`
- `fpga/rst_sync.v`
- `fpga/get_gpio.v`
- `fpga/packer8to32.v`
- `fpga/fifo_dualport.v`
- `fpga/fifo_singleclock.v`
- `fpga/sram_dualport.v`
- `fpga/fifo_fsm.v`
- `fpga/tx_write_guard.v`
- `fpga/host_cmd_ctrl.v`
- `fpga/status_ft_ctrl.v`
- `fpga/testbench.v`

Файл ограничений:

- `fpga/callistoS6.ucf`

Материалы reference и datasheet:

- `docs/`

