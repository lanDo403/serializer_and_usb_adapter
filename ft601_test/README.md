# FT601 Console App

## Назначение

`main_gpp.exe` — простая консольная утилита для ручной проверки `FT601` через `D3XX API`.

Утилита разделяет два типа операций:
- `raw payload` через `EP02` (`0x02`) и `EP82` (`0x82`);
- `service protocol` для управления текущей RTL-прошивкой FPGA и чтения статуса.

При старте программа:
- открывает устройство по `DEVICE_INDEX = 0`;
- выводит краткую информацию о выбранном устройстве;
- проверяет bulk pipe pair `0x02/0x82` через `FT_GetPipeInformation`;
- настраивает `FT_SetPipeTimeout` для обоих pipe;
- показывает flat console menu.


## Структура исходников

- `main.cpp` — меню, dispatch операций и retry/reopen flow.
- `ft601_device.h/.cpp` — открытие устройства, pipe discovery, raw D3XX read/write, reopen и pipe abort.
- `service_protocol.h/.cpp` — framed service protocol, opcodes, status frame и декодирование `status_word`.
- `payload_test.h/.cpp` — deterministic payload, raw test write и loopback integrity compare.
- `throughput.h/.cpp` — прикладной расчет скорости для payload-операций.

## Service protocol

Команда в FPGA передается двумя 32-битными словами по `EP02`:
1. `CMD_MAGIC = 0xA55A5AA5`
2. `opcode`

Ответ на `CMD_GET_STATUS` читается двумя 32-битными словами по `EP82`:
1. `STATUS_MAGIC = 0x5AA55AA5`
2. `status_word`

Поддерживаемые `opcode`:
- `CMD_CLR_TX_ERROR = 0x00000001`
- `CMD_CLR_RX_ERROR = 0x00000002`
- `CMD_CLR_ALL_ERROR = 0x00000003`
- `CMD_SET_LOOPBACK = 0xA5A50004`
- `CMD_SET_NORMAL = 0xA5A50005`
- `CMD_GET_STATUS = 0xA5A50006`

Формат `status_word`:
- `bit[0]` — `loopback_mode`
- `bit[1]` — `tx_error`
- `bit[2]` — `rx_error`
- `bit[3]` — `tx_fifo_empty`
- `bit[4]` — `tx_fifo_full`
- `bit[5]` — `loopback_fifo_empty`
- `bit[6]` — `loopback_fifo_full`
- `bit[31:7]` — `0`

Service-команды выполняются в stop-and-wait режиме. Для `SET_*` и `CLR_*` утилита сразу делает `GET_STATUS` как подтверждение.

## Меню

1. `Write test payload` — отправляет `64` 32-битных слова `1..64` в `EP02` и печатает скорость записи.
2. `Read payload to file` — читает raw payload из `EP82` до timeout/пустого чтения, сохраняет в `rx_dump.bin` и печатает скорость по фактически принятым байтам.
3. `Loopback integrity test` — включает loopback, пишет deterministic payload, читает ровно тот же размер, сравнивает данные и печатает write/read/round-trip throughput.
4. `Get FPGA status` — отправляет `CMD_GET_STATUS` и печатает `status_word`.
5. `Set loopback mode` — отправляет `CMD_SET_LOOPBACK`, затем автоматически читает статус.
6. `Set normal mode` — отправляет `CMD_SET_NORMAL`, затем автоматически читает статус.
7. `Clear TX error` — отправляет `CMD_CLR_TX_ERROR`, затем автоматически читает статус.
8. `Clear RX error` — отправляет `CMD_CLR_RX_ERROR`, затем автоматически читает статус.
9. `Clear all errors` — отправляет `CMD_CLR_ALL_ERROR`, затем автоматически читает статус.
10. `Exit`

Важно:
- `Read payload to file` — это raw dump, а не чтение статуса;
- status frame читается только через `Get FPGA status` или автоматический `GET_STATUS` после service-команды;
- `Loopback integrity test` по умолчанию использует `1024` слова, максимум `1048576` слов.

## Loopback integrity test

Сценарий проверки:
1. `CMD_SET_LOOPBACK`.
2. `CMD_GET_STATUS` и проверка `loopback_mode = 1`.
3. Генерация deterministic payload.
4. Запись payload в `EP02`.
5. Чтение ровно такого же количества слов из `EP82`.
6. Сравнение `tx payload` и `rx payload`.

При mismatch печатается первый word index, byte offset, expected и actual value.

## Throughput

Скорость считается только для payload-операций. Service-команды и status frame слишком маленькие, поэтому программа не использует их как benchmark.

`Write test payload` показывает host-to-device скорость. `Read payload to file` показывает device-to-host скорость по байтам, которые реально попали в `rx_dump.bin`. `Loopback integrity test` отдельно печатает скорость записи, скорость чтения и суммарный round-trip throughput.

Это прикладная оценка для текущего blocking D3XX flow, а не точный USB benchmark. На маленьких payload результат будет шумным, потому что накладные расходы Windows, D3XX и консольного приложения сравнимы с самой передачей. Если чтение завершилось timeout без payload-байтов, скорость не считается.

## Требования

- Windows.
- Установленный D3XX драйвер для FT601.
- `FTD3XXWU.dll` доступна рядом с `.exe` или через `PATH`.
- Компилятор и import library должны быть одной архитектуры.

В проекте библиотека лежит в `WU_FTD3XXLib\Lib\Dynamic\x64`, поэтому компилятор тоже должен быть `x64`.
Если использовать 32-битный `g++`, линковка с `x64` библиотекой не пройдет.

## Сборка

Проверенная команда сборки для `MSYS2 MinGW x64`:

```powershell
cd C:\Users\userIvan\Desktop\my_projects\logic_analyzer\ft601_test
& 'C:\msys64\mingw64\bin\g++.exe' -std=c++11 -Wall -Wextra -pedantic main.cpp ft601_device.cpp service_protocol.cpp payload_test.cpp throughput.cpp -I. -L.\WU_FTD3XXLib\Lib\Dynamic\x64 -lFTD3XXWU -o main_gpp.exe
```

## Запуск

```powershell
cd C:\Users\userIvan\Desktop\my_projects\logic_analyzer\ft601_test
.\main_gpp.exe
```

## Обработка ошибок

- При ошибке записи выполняется `FT_AbortPipe(0x02)`.
- При ошибке чтения, timeout или protocol error выполняется `FT_AbortPipe(0x82)`.
- При disconnect-статусах (`FT_DEVICE_NOT_CONNECTED`, `FT_DEVICE_NOT_FOUND`, `FT_INVALID_HANDLE`) утилита делает попытку reopen и повторяет операцию один раз.
- Если первым словом status frame пришел не `STATUS_MAGIC`, это protocol error.
- Физический `RESET_N` FT601 не управляется этой программой; он связан с `FPGA_RESET` в RTL.
