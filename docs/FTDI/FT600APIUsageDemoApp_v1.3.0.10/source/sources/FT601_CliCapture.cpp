/*
 * Simple FT601 CLI capture tool for FPGA -> USB (245 sync FIFO mode).
 *
 * Notes:
 * 1) TXE_N is not directly controlled from host API. Host starts FT_ReadPipe(),
 *    FT601 consumes IN endpoint data, and TXE_N behavior follows FT601 FIFO state.
 * 2) Build this file with FTD3XX SDK include/lib paths:
 *    - include: path to FTD3XX.h
 *    - lib: path to FTD3XX.lib
 */

#include <Windows.h>
#include <stdint.h>

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../../../FTD3XXLibrary/FTD3XX.h"

namespace {

constexpr UCHAR kPipeIn = 0x82;        // Channel 1 IN endpoint.
constexpr ULONG kDefaultChunk = 4096;  // Read request size in bytes.
constexpr ULONG kPipeTimeoutMs = 200;  // Keep reads responsive for CLI control.
constexpr DWORD kReenumWaitMs = 5000;  // Wait after FT_SetChipConfiguration.

void PrintHelp() {
  std::cout
      << "Commands:\n"
      << "  help\n"
      << "  config245                Configure FT601 to 245 mode + 1 channel\n"
      << "  showcfg                  Show current FIFO mode/channel config\n"
      << "  capture <txt> <sec> [chunk]\n"
      << "                           Read IN pipe and save as hex words to txt\n"
      << "  reopen [index]           Reopen device by index (default 0)\n"
      << "  quit\n";
}

bool ParseUlong(const std::string& s, ULONG* out) {
  if (out == nullptr || s.empty()) {
    return false;
  }
  try {
    size_t pos = 0;
    unsigned long value = std::stoul(s, &pos, 0);
    if (pos != s.size()) {
      return false;
    }
    *out = static_cast<ULONG>(value);
    return true;
  } catch (...) {
    return false;
  }
}

bool ParseDword(const std::string& s, DWORD* out) {
  if (out == nullptr || s.empty()) {
    return false;
  }
  try {
    size_t pos = 0;
    unsigned long value = std::stoul(s, &pos, 0);
    if (pos != s.size()) {
      return false;
    }
    *out = static_cast<DWORD>(value);
    return true;
  } catch (...) {
    return false;
  }
}

std::vector<std::string> Split(const std::string& line) {
  std::vector<std::string> tokens;
  std::istringstream iss(line);
  std::string token;
  while (iss >> token) {
    tokens.push_back(token);
  }
  return tokens;
}

class FtDevice {
 public:
  FtDevice() : handle_(nullptr), index_(0) {}
  ~FtDevice() { Close(); }

  bool Open(DWORD index) {
    Close();
    FT_STATUS st = FT_Create(reinterpret_cast<PVOID>(static_cast<ULONG_PTR>(index)),
                             FT_OPEN_BY_INDEX, &handle_);
    if (FT_FAILED(st)) {
      std::cerr << "FT_Create failed, status=" << st << "\n";
      return false;
    }
    index_ = index;
    FT_STATUS timeout_st = FT_SetPipeTimeout(handle_, kPipeIn, kPipeTimeoutMs);
    if (FT_FAILED(timeout_st)) {
      std::cerr << "FT_SetPipeTimeout warning, status=" << timeout_st << "\n";
    }
    return true;
  }

  void Close() {
    if (handle_ != nullptr) {
      FT_Close(handle_);
      handle_ = nullptr;
    }
  }

  bool IsOpen() const { return handle_ != nullptr; }
  FT_HANDLE Get() const { return handle_; }
  DWORD Index() const { return index_; }

 private:
  FT_HANDLE handle_;
  DWORD index_;
};

bool ShowConfiguration(FT_HANDLE h) {
  FT_60XCONFIGURATION cfg = {};
  FT_STATUS st = FT_GetChipConfiguration(h, &cfg);
  if (FT_FAILED(st)) {
    std::cerr << "FT_GetChipConfiguration failed, status=" << st << "\n";
    return false;
  }

  std::cout << "Current config:\n";
  std::cout << "  FIFOMode      : " << static_cast<int>(cfg.FIFOMode) << "\n";
  std::cout << "  ChannelConfig : " << static_cast<int>(cfg.ChannelConfig) << "\n";
  std::cout << "  FIFOClock     : " << static_cast<int>(cfg.FIFOClock) << "\n";
  std::cout << "  OptionalFeat  : 0x" << std::hex << std::uppercase
            << cfg.OptionalFeatureSupport << std::dec << "\n";
  return true;
}

bool Configure245SyncFifo(FtDevice* dev) {
  if (dev == nullptr || !dev->IsOpen()) {
    return false;
  }

  FT_60XCONFIGURATION cfg = {};
  FT_STATUS st = FT_GetChipConfiguration(dev->Get(), &cfg);
  if (FT_FAILED(st)) {
    std::cerr << "FT_GetChipConfiguration failed, status=" << st << "\n";
    return false;
  }

  cfg.FIFOMode = CONFIGURATION_FIFO_MODE_245;
  cfg.ChannelConfig = CONFIGURATION_CHANNEL_CONFIG_1;
#ifdef CONFIGURATION_OPTIONAL_FEATURE_DISABLECANCELSESSIONUNDERRUN
  cfg.OptionalFeatureSupport = CONFIGURATION_OPTIONAL_FEATURE_DISABLECANCELSESSIONUNDERRUN;
#endif

  st = FT_SetChipConfiguration(dev->Get(), &cfg);
  if (FT_FAILED(st)) {
    std::cerr << "FT_SetChipConfiguration failed, status=" << st << "\n";
    return false;
  }

  std::cout << "Configuration written. Re-enumeration wait " << kReenumWaitMs
            << " ms...\n";
  const DWORD idx = dev->Index();
  dev->Close();
  Sleep(kReenumWaitMs);
  return dev->Open(idx);
}

bool CaptureToTxt(FT_HANDLE h, const std::string& path, DWORD seconds, ULONG chunk_bytes) {
  if (h == nullptr) {
    return false;
  }
  if (seconds == 0) {
    std::cerr << "Seconds must be > 0\n";
    return false;
  }
  if (chunk_bytes == 0 || chunk_bytes > (1u << 20)) {
    std::cerr << "Chunk must be in range [1..1048576]\n";
    return false;
  }

  std::ofstream out(path.c_str(), std::ios::out | std::ios::trunc);
  if (!out.is_open()) {
    std::cerr << "Cannot open output file: " << path << "\n";
    return false;
  }

  out << std::hex << std::uppercase << std::setfill('0');

  std::vector<UCHAR> buf(chunk_bytes, 0);
  uint64_t total_bytes = 0;
  uint64_t total_words = 0;
  unsigned consecutive_errors = 0;

  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
  while (std::chrono::steady_clock::now() < deadline) {
    ULONG got = 0;
    FT_STATUS st = FT_ReadPipe(h, kPipeIn, buf.data(), chunk_bytes, &got, nullptr);
    if (FT_FAILED(st)) {
      ++consecutive_errors;
      if (consecutive_errors >= 50) {
        std::cerr << "Too many read errors, last status=" << st << "\n";
        return false;
      }
      continue;
    }
    consecutive_errors = 0;
    if (got == 0) {
      continue;
    }

    total_bytes += got;

    ULONG i = 0;
    for (; i + 3 < got; i += 4) {
      uint32_t w = static_cast<uint32_t>(buf[i]) |
                   (static_cast<uint32_t>(buf[i + 1]) << 8) |
                   (static_cast<uint32_t>(buf[i + 2]) << 16) |
                   (static_cast<uint32_t>(buf[i + 3]) << 24);
      out << std::setw(8) << w << "\n";
      ++total_words;
    }
    for (; i < got; ++i) {
      out << "B " << std::setw(2) << static_cast<unsigned>(buf[i]) << "\n";
    }
  }

  out.close();
  std::cout << "Capture done: " << total_bytes << " bytes, " << total_words
            << " words written to " << path << "\n";
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  FtDevice dev;
  if (!dev.Open(0)) {
    std::cerr << "Open by index 0 failed. Check FTDI driver/device.\n";
  } else {
    std::cout << "Connected to FT60x index 0\n";
  }

  if (argc > 1) {
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
      args.push_back(argv[i]);
    }

    if (args[0] == "config245") {
      if (!dev.IsOpen()) {
        return 1;
      }
      return Configure245SyncFifo(&dev) ? 0 : 2;
    }

    if (args[0] == "capture") {
      if (args.size() < 3) {
        std::cerr << "Usage: capture <txt> <sec> [chunk]\n";
        return 1;
      }
      DWORD sec = 0;
      ULONG chunk = kDefaultChunk;
      if (!ParseDword(args[2], &sec)) {
        std::cerr << "Invalid <sec>\n";
        return 1;
      }
      if (args.size() >= 4 && !ParseUlong(args[3], &chunk)) {
        std::cerr << "Invalid [chunk]\n";
        return 1;
      }
      if (!dev.IsOpen()) {
        return 1;
      }
      return CaptureToTxt(dev.Get(), args[1], sec, chunk) ? 0 : 3;
    }

    if (args[0] == "showcfg") {
      if (!dev.IsOpen()) {
        return 1;
      }
      return ShowConfiguration(dev.Get()) ? 0 : 4;
    }

    PrintHelp();
    return 1;
  }

  PrintHelp();
  std::string line;
  while (true) {
    std::cout << "\nft601> ";
    if (!std::getline(std::cin, line)) {
      break;
    }
    auto cmd = Split(line);
    if (cmd.empty()) {
      continue;
    }

    if (cmd[0] == "quit" || cmd[0] == "exit") {
      break;
    } else if (cmd[0] == "help") {
      PrintHelp();
    } else if (cmd[0] == "reopen") {
      DWORD idx = 0;
      if (cmd.size() >= 2 && !ParseDword(cmd[1], &idx)) {
        std::cerr << "Invalid index\n";
        continue;
      }
      if (dev.Open(idx)) {
        std::cout << "Reopened index " << idx << "\n";
      }
    } else if (cmd[0] == "showcfg") {
      if (!dev.IsOpen()) {
        std::cerr << "Device not open\n";
        continue;
      }
      ShowConfiguration(dev.Get());
    } else if (cmd[0] == "config245") {
      if (!dev.IsOpen()) {
        std::cerr << "Device not open\n";
        continue;
      }
      Configure245SyncFifo(&dev);
    } else if (cmd[0] == "capture") {
      if (!dev.IsOpen()) {
        std::cerr << "Device not open\n";
        continue;
      }
      if (cmd.size() < 3) {
        std::cerr << "Usage: capture <txt> <sec> [chunk]\n";
        continue;
      }
      DWORD sec = 0;
      ULONG chunk = kDefaultChunk;
      if (!ParseDword(cmd[2], &sec)) {
        std::cerr << "Invalid <sec>\n";
        continue;
      }
      if (cmd.size() >= 4 && !ParseUlong(cmd[3], &chunk)) {
        std::cerr << "Invalid [chunk]\n";
        continue;
      }
      CaptureToTxt(dev.Get(), cmd[1], sec, chunk);
    } else {
      std::cerr << "Unknown command: " << cmd[0] << "\n";
      PrintHelp();
    }
  }

  dev.Close();
  return 0;
}
