# Система высокоскоростного обмена данными на базе ПЛИС с использованием интерфейса USB 3.0

Этот репозиторий содержит RTL, ограничения и документацию для тракта обмена данными на базе `Xilinx Spartan-6` и `FTDI FT601` в режиме `245 synchronous FIFO`.

Основной сценарий проекта:

- принять 8-битный поток по GPIO;
- упаковать его в 32-битные слова;
- передать данные на ПК через `USB 3.0`.

Дополнительно в том же bitstream реализован runtime loopback-режим для проверки FT601 без внешнего источника данных на GPIO.

## Что умеет проект

- передавать поток `GPIO -> FT601 -> PC`;
- принимать служебные команды от ПК через FT601 RX path;
- включать loopback по команде без перепрошивки;
- возвращать данные `PC -> FT601 -> FPGA -> FT601 -> PC` в одном и том же bitstream;
- удерживать корректную FT601 burst-фазировку на RX и TX путях.

## Режимы работы

### Normal mode

Режим по умолчанию после reset.

Путь данных:

`GPIO -> get_gpio -> packer8to32 -> fifo_tx -> fifo_fsm -> FT601 -> PC`

Особенности:

- полезные данные приходят с GPIO;
- FT601 RX используется только для служебных команд;
- GPIO запись в TX FIFO блокируется при активном loopback.

### FT loopback mode

Включается командой `32'hA5A50004` через FT601 RX path.

Путь данных:

`PC -> FT601 -> fifo_fsm -> loopback_fifo -> fifo_fsm -> FT601 -> PC`

Особенности:

- loopback работает в одном bitstream с normal mode;
- payload хранится как `{BE, DATA}` в `fpga/fifo_singleclock.v`;
- выход из loopback выполняется через `FPGA_RESET`.

## Структура репозитория

### `fpga/`

Основные HDL-файлы проекта.

Ключевые модули:

- `top.v` — верхний уровень;
- `bit_sync.v` — двухтактный синхронизатор одиночного бита между доменами;
- `ft601_io.v` — физическая обвязка FT601 и входная регистрация `TXE_N/RXF_N`;
- `fifo_fsm.v` — handshake и burst-логика FT601;
- `fifo_dualport.v` + `sram_dualport.v` — асинхронный TX FIFO между GPIO и FT domain;
- `fifo_singleclock.v` — loopback FIFO в домене `ft_clk_i`;
- `loopback_ft_ctrl.v` — FT-domain логика захвата RX-слов и управления loopback/TX prefetch;
- `host_cmd_ctrl.v` — потоковый декодер команд FT601 RX path;
- `tx_write_guard.v` — управление записью в TX FIFO и sticky TX error;
- `rst_sync.v` — синхронизация reset по доменам;
- `testbench.v` — основной проверочный стенд;
- `callistoS6.ucf` — pinout и timing constraints для Callisto S6.

### `docs/`

Справочные материалы и проектная документация.

Что лежит в папке:

- `SPECIFICATION.md` — техническая спецификация текущей архитектуры и требований к поведению;
- datasheet FT601;
- application note FTDI по master FIFO;
- reference design FTDI для Spartan-6;
- vendor Windows utilities `WU_*` для настройки и ручной проверки FT601;
- дополнительные материалы по FIFO, timing и ISE.

### `ft601_test/`

Консольная C++ программа для ручной работы с FT601 через D3XX API на стороне ПК.

Что лежит в папке:

- `main.cpp` — исходник консольной утилиты;
- `README.md` — краткий runbook по сборке и запуску;
- `WU_FTD3XXLib/` — библиотека D3XX для сборки и запуска;
- `WU_FTD3XX_Driver/` — драйвер D3XX для FT601 под Windows.

## Как пользоваться проектом

### Работа на FPGA

Базовый сценарий на железе:

1. Собрать и прошить FPGA.
2. Настроить FT601 в `245 synchronous FIFO mode`.
3. После reset дизайн находится в `normal mode`.
4. В `normal mode` подавать полезные данные на GPIO и читать их на стороне ПК через FT601.
5. Для входа в loopback отправить в FT601 слово `32'hA5A50004` с `BE = 4'hF`.
6. После этого передавать payload в FT601 RX path и читать его обратно через FT601 TX path.
7. Для выхода из loopback подать `FPGA_RESET`.

### Проверка со стороны ПК через `ft601_test`

Если нужен прямой тест обмена без отдельного GUI-приложения FTDI:

1. Собрать или запустить `ft601_test/main_gpp.exe`.
2. Выбрать `Write counter 1..10000`, чтобы отправить тестовый поток в `EP02` (`0x02`).
3. Выбрать `Read to file`, чтобы прочитать данные из `EP82` (`0x82`) в `rx_dump.bin`.
4. Для loopback сначала включить runtime loopback в FPGA, затем писать в `EP02` и читать ответ из `EP82`.

### Что важно учитывать

- один bitstream обслуживает оба режима;
- FT601 RX path в `normal mode` предназначен для служебных команд;
- loopback payload не должен смешиваться с командным потоком;
- `TXE_N` и `RXF_N` внутри дизайна используются только в зарегистрированном виде после `ft601_io`.

## Куда смотреть дальше

Если нужен быстрый вход в проект:

1. `README.md` — общая карта репозитория и сценарии использования.
2. `docs/SPECIFICATION.md` — точные требования к архитектуре, handshake и verification.
3. `fpga/top.v` — верхний уровень и реальный datapath.
4. `fpga/testbench.v` — проверка текущего поведения дизайна.
5. `ft601_test/README.md` — host-side проверка FT601 через D3XX.

