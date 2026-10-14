#include "debug/dev_args.h"

#include <cctype>

namespace tomba {

std::vector<std::string_view> DevArgs::words(std::string_view line) {
  std::vector<std::string_view> result;
  std::size_t at = 0;
  while (at < line.size()) {
    while (at < line.size() && std::isspace(static_cast<unsigned char>(line[at])) != 0) {
      ++at;
    }
    const std::size_t begin = at;
    while (at < line.size() && std::isspace(static_cast<unsigned char>(line[at])) == 0) {
      ++at;
    }
    if (at > begin) {
      result.push_back(line.substr(begin, at - begin));
    }
  }
  return result;
}

bool DevArgs::decimal(std::string_view word, uint32_t &value) {
  if (word.empty()) {
    return false;
  }
  uint64_t parsed = 0;
  for (const char digit : word) {
    if (digit < '0' || digit > '9') {
      return false;
    }
    parsed = parsed * 10u + static_cast<uint64_t>(digit - '0');
    if (parsed > UINT32_MAX) {
      return false;
    }
  }
  value = static_cast<uint32_t>(parsed);
  return true;
}

bool DevArgs::integer(std::string_view word, int32_t &value) {
  const bool negative = word.starts_with('-');
  uint32_t magnitude = 0;
  if (!decimal(negative ? word.substr(1) : word, magnitude) || magnitude > (negative ? 0x80000000u : 0x7FFFFFFFu)) {
    return false;
  }
  value = negative ? static_cast<int32_t>(0u - magnitude) : static_cast<int32_t>(magnitude);
  return true;
}

} // namespace tomba
