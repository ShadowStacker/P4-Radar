#pragma once

#include <cstddef>
#include <cstring>
#include <vector>

namespace core {

/**
 * A saved aircraft. Identified by its ICAO 24-bit address, which belongs to
 * the airframe (callsigns change from flight to flight). The other fields are
 * remembered from when it was saved, for display while it's out of range.
 */
struct Favorite {
  char hex[8] = "";
  char label[12] = "";  // callsign or registration when saved
  char type[5] = "";
};

constexpr size_t kMaxFavorites = 32;

inline bool containsHex(const std::vector<Favorite>& favs, const char* hex) {
  for (const Favorite& f : favs) {
    if (std::strcmp(f.hex, hex) == 0) return true;
  }
  return false;
}

}  // namespace core
