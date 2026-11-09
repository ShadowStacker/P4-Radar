#pragma once

#include <cstdint>

// The map overlay's built-in data (scripts/build_geo.py): a coarse world cut
// into 5 x 5 degree cells, and which cells have a detailed file to download.
namespace data {

extern const uint8_t kGeoDetailCells[];    // bitset over core::geo cell ids
extern const uint32_t kGeoWorldOffset[];   // cell count + 1 entries
extern const uint8_t kGeoWorld[];          // raw-deflated coarse cells

}  // namespace data
