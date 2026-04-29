#include "service_protocol.h"

#include "ft601_device.h"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

namespace {

void PrintHex32(const char* label, uint32_t value) {
    std::cout << label << "0x" << std::hex << std::setw(8) << std::setfill('0')
              << value << std::dec << std::setfill(' ') << "\n";
}

}  // namespace

bool SendCommandFrame(FT_HANDLE h,
                      uint32_t opcode,
                      std::string& err,
                      FT_STATUS* last_status) {
    const std::vector<uint32_t> frame = {CMD_MAGIC, opcode};
    return WriteWords(h, frame, err, last_status);
}

bool ReadStatusFrame(FT_HANDLE h,
                     uint32_t& status_word,
                     std::string& err,
                     FT_STATUS* last_status) {
    std::vector<uint32_t> frame;
    if (!ReadExactWords(h, 2, frame, err, last_status)) {
        return false;
    }

    if (frame[0] != STATUS_MAGIC) {
        if (last_status != nullptr) {
            *last_status = FT_OTHER_ERROR;
        }
        AbortPipeBestEffort(h, IN_PIPE);
        std::ostringstream oss;
        oss << "Protocol error: expected STATUS_MAGIC 0x" << std::hex
            << std::setw(8) << std::setfill('0') << STATUS_MAGIC
            << ", got 0x" << std::setw(8) << frame[0];
        err = oss.str();
        return false;
    }

    status_word = frame[1];
    return true;
}

bool RequestStatus(FT_HANDLE h,
                   uint32_t& status_word,
                   std::string& err,
                   FT_STATUS* last_status) {
    if (!SendCommandFrame(h, CMD_GET_STATUS, err, last_status)) {
        return false;
    }

    return ReadStatusFrame(h, status_word, err, last_status);
}

bool DoGetStatus(FT_HANDLE h, std::string& err, FT_STATUS* last_status) {
    uint32_t status_word = 0;
    if (!RequestStatus(h, status_word, err, last_status)) {
        return false;
    }
    PrintStatusWord(status_word);
    return true;
}

bool DoCommandAndGetStatus(FT_HANDLE h,
                           uint32_t opcode,
                           std::string& err,
                           FT_STATUS* last_status) {
    if (!SendCommandFrame(h, opcode, err, last_status)) {
        return false;
    }

    uint32_t status_word = 0;
    if (!RequestStatus(h, status_word, err, last_status)) {
        return false;
    }
    PrintStatusWord(status_word);
    return true;
}

void PrintStatusWord(uint32_t status_word) {
    PrintHex32("Status word: ", status_word);
    std::cout << "  mode              : "
              << ((status_word & (1u << 0)) ? "loopback" : "normal") << "\n";
    std::cout << "  tx_error          : "
              << ((status_word & (1u << 1)) ? "1" : "0") << "\n";
    std::cout << "  rx_error          : "
              << ((status_word & (1u << 2)) ? "1" : "0") << "\n";
    std::cout << "  tx_fifo_empty     : "
              << ((status_word & (1u << 3)) ? "1" : "0") << "\n";
    std::cout << "  tx_fifo_full      : "
              << ((status_word & (1u << 4)) ? "1" : "0") << "\n";
    std::cout << "  loopback_empty    : "
              << ((status_word & (1u << 5)) ? "1" : "0") << "\n";
    std::cout << "  loopback_full     : "
              << ((status_word & (1u << 6)) ? "1" : "0") << "\n";
}