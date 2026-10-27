#pragma once

#include <cstddef>
#include <cstdint>

namespace data {

enum AirportClass : uint8_t { kLarge = 0, kMedium = 1, kSmall = 2 };

struct Airport {
  char ident[8];  // ICAO, else GPS/FAA code (e.g. "KBDN", "S39", "OR42")
  char iata[4];
  char name[33];
  int32_t lat_e7;
  int32_t lon_e7;
  uint8_t cls;  // AirportClass
};

enum RunwayFlags : uint8_t {
  kRunwayPaved = 1,
  kRunwayApprox = 2,  // no surveyed ends; drawn through the airport on its designator heading
};

struct Runway {
  uint16_t airport_idx;
  int32_t le_lat_e7;
  int32_t le_lon_e7;
  int32_t he_lat_e7;
  int32_t he_lon_e7;
  uint16_t length_m;
  uint8_t flags;  // RunwayFlags
};

// kAirports is sorted by latitude; kRunways is sorted by airport_idx.
extern const size_t kAirportCount;
extern const size_t kRunwayCount;
extern const Airport kAirports[];
extern const Runway kRunways[];

}  // namespace data
