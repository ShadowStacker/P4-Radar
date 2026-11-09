#include "data/airports.h"

#include "core/inflate.h"

namespace data {

std::string airportName(size_t idx) {
  if (idx >= kAirportCount) return "";
  const size_t b = idx / kNameBlockSize;
  std::string block;
  if (!core::inflateRaw(kAirportNames + kAirportNameBlock[b], kAirportNameBlock[b + 1] - kAirportNameBlock[b], &block,
                        kNameBlockSize * 40)) {
    return "";
  }
  size_t pos = 0;
  for (size_t n = idx % kNameBlockSize; n > 0; --n) {
    pos = block.find('\0', pos);
    if (pos == std::string::npos) return "";
    ++pos;
  }
  const size_t end = block.find('\0', pos);
  return block.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
}

}  // namespace data
