#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace data {

enum AirportClass : uint8_t { kLarge = 0, kMedium = 1, kSmall = 2 };

/** 20 bytes per airport; the names are kept apart, compressed (see airportName()). */
struct Airport {
  int32_t lat_e7;
  int32_t lon_e7;
  char ident[8];  // ICAO, else GPS/FAA code (e.g. "KBDN", "S39", "OR42"), NUL-terminated
  char iata[3];   // up to 3 letters, NOT NUL-terminated (use iataCode())
  uint8_t cls;    // AirportClass
};

enum RunwayFlags : uint8_t {
  kRunwayPaved = 1,
  kRunwayApprox = 2,  // no surveyed ends; drawn through the airport on its designator heading
};

/** 14 bytes per runway: its ends as offsets from its airport, 1e-5 degree (about 1 m) steps. */
struct Runway {
  uint16_t airport_idx;
  int16_t le_dlat, le_dlon;
  int16_t he_dlat, he_dlon;
  uint16_t length_m;
  uint8_t flags;  // RunwayFlags
};

// kAirports is sorted by latitude; kRunways is sorted by airport_idx.
extern const size_t kAirportCount;
extern const size_t kRunwayCount;
extern const Airport kAirports[];
extern const Runway kRunways[];

// Names: blocks of kNameBlockSize names ('\0'-separated), each raw-deflated.
// Block b is kAirportNames[kAirportNameBlock[b] .. kAirportNameBlock[b + 1]).
constexpr size_t kNameBlockSize = 256;
extern const uint8_t kAirportNames[];
extern const uint32_t kAirportNameBlock[];

/** The IATA code as a C string ("" if none). */
struct IataCode {
  char s[4];
};
inline IataCode iataCode(const Airport& a) {
  IataCode c{{a.iata[0], a.iata[1], a.iata[2], '\0'}};
  return c;
}

/** A runway end in 1e-7 degrees (`high` = the "he" end). */
inline int32_t runwayLatE7(const Runway& r, bool high) {
  return kAirports[r.airport_idx].lat_e7 + (high ? r.he_dlat : r.le_dlat) * 100;
}
inline int32_t runwayLonE7(const Runway& r, bool high) {
  int64_t v = static_cast<int64_t>(kAirports[r.airport_idx].lon_e7) + (high ? r.he_dlon : r.le_dlon) * 100;
  if (v > 1800000000) v -= 3600000000LL;
  if (v < -1800000000) v += 3600000000LL;
  return static_cast<int32_t>(v);
}

/** The airport's name ("" if `idx` is out of range). Unpacks one block of names, so not for every frame. */
std::string airportName(size_t idx);

}  // namespace data
