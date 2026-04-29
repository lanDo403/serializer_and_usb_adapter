#pragma once

#include <cstdint>
#include <string>
#include <FTD3XX.h>

constexpr uint32_t CMD_MAGIC = 0xA55A5AA5u;
constexpr uint32_t STATUS_MAGIC = 0x5AA55AA5u;
constexpr uint32_t CMD_CLR_TX_ERROR = 0x00000001u;
constexpr uint32_t CMD_CLR_RX_ERROR = 0x00000002u;
constexpr uint32_t CMD_CLR_ALL_ERROR = 0x00000003u;
constexpr uint32_t CMD_SET_LOOPBACK = 0xA5A50004u;
constexpr uint32_t CMD_SET_NORMAL = 0xA5A50005u;
constexpr uint32_t CMD_GET_STATUS = 0xA5A50006u;

bool SendCommandFrame(FT_HANDLE h,
                      uint32_t opcode,
                      std::string& err,
                      FT_STATUS* last_status);

bool ReadStatusFrame(FT_HANDLE h,
                     uint32_t& status_word,
                     std::string& err,
                     FT_STATUS* last_status);

bool RequestStatus(FT_HANDLE h,
                   uint32_t& status_word,
                   std::string& err,
                   FT_STATUS* last_status);

bool DoGetStatus(FT_HANDLE h, std::string& err, FT_STATUS* last_status);

bool DoCommandAndGetStatus(FT_HANDLE h,
                           uint32_t opcode,
                           std::string& err,
                           FT_STATUS* last_status);

void PrintStatusWord(uint32_t status_word);