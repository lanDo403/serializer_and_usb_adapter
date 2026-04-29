#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <FTD3XX.h>

constexpr UCHAR OUT_PIPE = 0x02;
constexpr UCHAR IN_PIPE = 0x82;
constexpr ULONG TIMEOUT_MS = 2000;
constexpr ULONG CHUNK_BYTES = 1u << 20;
constexpr DWORD DEVICE_INDEX = 0;

std::string StatusToStr(FT_STATUS st);
bool IsDisconnectStatus(FT_STATUS st);
void AbortPipeBestEffort(FT_HANDLE h, UCHAR pipe_id);

bool OpenDevice(FT_HANDLE& h, std::string& err);
bool ReopenDevice(FT_HANDLE& h, std::string& err);

bool WriteWords(FT_HANDLE h,
                const std::vector<uint32_t>& words,
                std::string& err,
                FT_STATUS* last_status);

bool ReadExactWords(FT_HANDLE h,
                    size_t count,
                    std::vector<uint32_t>& words,
                    std::string& err,
                    FT_STATUS* last_status);

bool DoReadToFile(FT_HANDLE h,
                  const std::string& path,
                  std::string& err,
                  uint64_t& out_bytes,
                  FT_STATUS* last_status);