#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

/**
 * The map overlay's shapes (country borders, state/province lines,
 * coastlines and controlled airspace) in 5 x 5 degree cells. The file format
 * is described in scripts/build_geo.py.
 */
namespace core::geo {

constexpr int kCellDeg = 5;
constexpr int kRows = 180 / kCellDeg, kCols = 360 / kCellDeg;
constexpr int kCells = kRows * kCols;

/** The cell holding a point (latitude clamped, longitude wrapped). */
int cellOf(double lat, double lon);
/** Cells overlapping a box; lon_lo > lon_hi or a span over 360 wraps around. */
std::vector<int> cellsCovering(double lat_lo, double lat_hi, double lon_lo, double lon_hi);

enum class Kind : uint8_t { Border = 0, State = 1, Coast = 2, Airspace = 3 };
enum class AirspaceClass : uint8_t { B = 0, C = 1, D = 2 };

struct Line {
  Kind kind = Kind::Border;
  AirspaceClass cls = AirspaceClass::B;  // airspace only
  uint16_t floor_100ft = 0, ceil_100ft = 0;  // airspace only; floor 0 = surface
  int32_t lat_min = 0, lat_max = 0, lon_min = 0, lon_max = 0;  // 1e-7 degree
  uint32_t first = 0, count = 0;  // points: pts[2 * first ..], lat then lon
};

struct Shapes {
  std::vector<Line> lines;
  std::vector<int32_t> pts;  // lat, lon pairs in 1e-7 degree
  size_t bytes() const { return lines.size() * sizeof(Line) + pts.size() * sizeof(int32_t); }
};

/** Parses an inflated cell. False (and `out` cleared) if it's malformed. */
bool parse(int cell, const uint8_t* data, size_t len, Shapes* out);
/** Inflates (raw DEFLATE) and parses a stored cell. */
bool decode(int cell, const uint8_t* data, size_t len, Shapes* out);

}  // namespace core::geo
