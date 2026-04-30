# FT601 Console App

`main_gpp.exe` - небольшая консольная утилита для ручной проверки FT601 через D3XX API. Она не пытается быть полноценным CLI-инструментом: запускается, открывает устройство `DEVICE_INDEX = 0`, проверяет pipe `0x02/0x82`, настраивает таймауты и показывает меню.

Программа работает с двумя типами трафика. Raw payload идет напрямую через `EP02` и `EP82`. Service traffic использует framed protocol текущей RTL-прошивки: команда отправляется как `CMD_MAGIC + opcode`, а статус читается как `STATUS_MAGIC + status_word`. Эти два режима лучше не смешивать в одном ручном сценарии.

## Структура кода

Код разделен на несколько небольших файлов, чтобы `main.cpp` оставался читаемым. В `main.cpp` находится меню и обработка выбранных действий. `ft601_device.h/.cpp` отвечает за открытие устройства, поиск pipe, D3XX read/write, abort и reopen. `service_protocol.h/.cpp` содержит opcodes, отправку service frame, чтение status frame и печать `status_word`. `payload_test.h/.cpp` генерирует тестовые данные и выполняет loopback integrity compare.

## Service protocol

Команда в FPGA передается двумя 32-битными словами по `EP02`:

```text
CMD_MAGIC = 0xA55A5AA5
opcode
```

Ответ на `CMD_GET_STATUS` читается из `EP82`:

```text
STATUS_MAGIC = 0x5AA55AA5
status_word
```

Поддерживаемые команды: `CMD_SET_LOOPBACK`, `CMD_SET_NORMAL`, `CMD_GET_STATUS`, `CMD_CLR_TX_ERROR`, `CMD_CLR_RX_ERROR`, `CMD_CLR_ALL_ERROR`. Service-команды выполняются в stop-and-wait режиме. Для `SET_*` и `CLR_*` программа сразу делает `GET_STATUS`, чтобы показать подтверждение от FPGA.

`status_word` печатается в hex и расшифровывается по битам: текущий режим, `tx_error`, `rx_error`, empty/full normal TX FIFO и empty/full loopback FIFO.

## Меню

После успешного открытия устройства программа показывает меню:

```text
1) Write test payload
2) Read payload to file
3) Loopback integrity test
4) Get FPGA status
5) Set loopback mode
6) Set normal mode
7) Clear TX error
8) Clear RX error
9) Clear all errors
10) Exit
```

`Write test payload` отправляет 64 тестовых 32-битных слова в `EP02`. `Read payload to file` читает raw payload из `EP82` до timeout или пустого чтения и сохраняет данные в `rx_dump.bin`. Это debug-операции, они не читают status frame.

Главная проверка для режима `PC -> FPGA -> PC` - `Loopback integrity test`. Утилита включает loopback, проверяет статус, генерирует deterministic payload, пишет его в `EP02`, читает ровно такой же объем из `EP82` и сравнивает слово в слово. По умолчанию используется 1024 слова. Если нажать Enter на вопросе размера, будет выбран этот default. При ошибке сравнения программа печатает первый word index, byte offset, expected и actual value.

## Сборка

Нужны Windows, установленный D3XX драйвер для FT601 и `MSYS2 MinGW x64`. Библиотека в проекте лежит в `WU_FTD3XXLib\Lib\Dynamic\x64`, поэтому компилятор тоже должен быть x64.

```powershell
cd C:\Users\userIvan\Desktop\my_projects\logic_analyzer\ft601_test
& 'C:\msys64\mingw64\bin\g++.exe' -std=c++11 -Wall -Wextra -pedantic main.cpp ft601_device.cpp service_protocol.cpp payload_test.cpp -I. -L.\WU_FTD3XXLib\Lib\Dynamic\x64 -lFTD3XXWU -o main_gpp.exe
```

Перед запуском `FTD3XXWU.dll` должна быть рядом с `.exe` или доступна через `PATH`.

```powershell
.\main_gpp.exe
```

## Рекомендуемый ручной сценарий

После прошивки FPGA сначала выбери `Get FPGA status`. Нормальный стартовый режим - `normal`. Затем выбери `Set loopback mode` и запусти `Loopback integrity test`. Если тест прошел, можно вернуть дизайн в `normal mode` через `Set normal mode`. Для recovery доступны `Clear TX error`, `Clear RX error` и `Clear all errors`.

Если устройство отключилось или D3XX вернул disconnect-статус, программа пытается выполнить reopen и повторить операцию один раз. При ошибке записи вызывается `FT_AbortPipe(0x02)`, при ошибке чтения или protocol error - `FT_AbortPipe(0x82)`. Если первым словом status frame пришел не `STATUS_MAGIC`, это считается ошибкой протокола.