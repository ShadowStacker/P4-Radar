#pragma once

#include <cstddef>
#include <cstdint>

namespace data {

struct Airport {
  char ident[5];  // ICAO (or local) code
  char iata[4];
  char name[41];
  int32_t lat_e7;
  int32_t lon_e7;
  uint8_t cls;  // 0 = large, 1 = medium
};

struct Runway {
  uint16_t airport_idx;
  int32_t le_lat_e7;
  int32_t le_lon_e7;
  int32_t he_lat_e7;
  int32_t he_lon_e7;
  uint16_t length_m;
};

extern const size_t kAirportCount;
extern const size_t kRunwayCount;
extern const Airport kAirports[];
extern const Runway kRunways[];

}  // namespace data
