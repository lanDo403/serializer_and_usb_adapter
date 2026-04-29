#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <FTD3XX.h>

constexpr uint32_t WRITE_WORD_COUNT = 64;
constexpr size_t DEFAULT_LOOPBACK_WORDS = 1024;
constexpr size_t MAX_LOOPBACK_WORDS = 1u << 20;  // 4 MiB payload

std::vector<uint32_t> GenerateDeterministicPayload(size_t word_count);

bool WritePayload(FT_HANDLE h,
                  const std::vector<uint32_t>& payload,
                  std::string& err,
                  FT_STATUS* last_status);

bool ComparePayload(const std::vector<uint32_t>& tx,
                    const std::vector<uint32_t>& rx,
                    std::string& err);

size_t ReadPayloadWordCount();

bool DoWriteTestPayload(FT_HANDLE h, std::string& err, FT_STATUS* last_status);

bool DoLoopbackIntegrityTest(FT_HANDLE h,
                             size_t word_count,
                             std::string& err,
                             FT_STATUS* last_status);