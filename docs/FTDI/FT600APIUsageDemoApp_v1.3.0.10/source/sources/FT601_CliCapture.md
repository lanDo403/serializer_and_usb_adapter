# FT601_CliCapture

Simple CLI utility for FT601 in `245 sync FIFO` mode.

## What it does
- Configures FT601 to `CONFIGURATION_FIFO_MODE_245` and `CONFIGURATION_CHANNEL_CONFIG_1`.
- Captures data from IN endpoint `0x82` using `FT_ReadPipe`.
- Saves incoming stream to `.txt` as hex words (`32-bit`, little-endian).

## Important note about TXE_N
- Host API does **not** directly force `TXE_N`.
- `TXE_N` behavior depends on FT601 FIFO state.
- When host reads IN pipe (`FT_ReadPipe`), FT601 drains internal USB/FIFO buffers, and FPGA can continue writing according to FT601 handshake timing.

## Build (Developer Command Prompt for VS)
From repo root:

```bat
cl /nologo /EHsc /std:c++17 ^
  FT600APIUsageDemoApp_v1.3.0.10\source\sources\FT601_CliCapture.cpp ^
  /I FTD3XXLibrary ^
  /link /LIBPATH:FTD3XXLibrary\x64\Static_Lib FTD3XX.lib ^
  /OUT:FT601_CliCapture.exe
```

For Win32 build, replace `x64\Static_Lib` with `Win32\Static_Lib`.

## Usage
```bat
FT601_CliCapture.exe
```

Commands in interactive mode:
- `showcfg`
- `config245`
- `capture dump.txt 10 4096`
- `reopen 0`
- `quit`

One-shot mode examples:
```bat
FT601_CliCapture.exe config245
FT601_CliCapture.exe capture dump.txt 10 4096
```

