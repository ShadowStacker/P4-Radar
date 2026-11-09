#include "core/geo_shapes.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#include "core/inflate.h"

namespace core::geo {

int cellOf(double lat, double lon) {
  int row = static_cast<int>(std::floor((lat + 90.0) / kCellDeg));
  row = std::clamp(row, 0, kRows - 1);
  double l = std::fmod(lon + 180.0, 360.0);
  if (l < 0) l += 360.0;
  const int col = std::clamp(static_cast<int>(std::floor(l / kCellDeg)), 0, kCols - 1);
  return row * kCols + col;
}

std::vector<int> cellsCovering(double lat_lo, double lat_hi, double lon_lo, double lon_hi) {
  std::vector<int> out;
  const int r0 = cellOf(lat_lo, 0) / kCols, r1 = cellOf(lat_hi, 0) / kCols;
  const bool all = lon_hi - lon_lo >= 360.0;
  const int c0 = all ? 0 : cellOf(0, lon_lo) % kCols;
  const int ncols = all ? kCols : (cellOf(0, lon_hi) % kCols - c0 + kCols) % kCols + 1;
  for (int r = r0; r <= r1; ++r) {
    for (int k = 0; k < ncols; ++k) out.push_back(r * kCols + (c0 + k) % kCols);
  }
  return out;
}

namespace {

struct Reader {
  const uint8_t* p;
  const uint8_t* end;
  bool ok = true;

  uint8_t byte() {
    if (p >= end) {
      ok = false;
      return 0;
    }
    return *p++;
  }
  uint64_t varint() {
    uint64_t v = 0;
    for (int shift = 0; shift < 64; shift += 7) {
      const uint8_t b = byte();
      if (!ok) return 0;
      v |= static_cast<uint64_t>(b & 0x7F) << shift;
      if (!(b & 0x80)) return v;
    }
    ok = false;
    return 0;
  }
  int64_t zigzag() {
    const uint64_t v = varint();
    return static_cast<int64_t>(v >> 1) ^ -static_cast<int64_t>(v & 1);
  }
};

}  // namespace

bool parse(int cell, const uint8_t* data, size_t len, Shapes* out) {
  out->lines.clear();
  out->pts.clear();
  if (cell < 0 || cell >= kCells || len < 4 || std::memcmp(data, "PRG", 3) != 0) return false;
  int64_t step;  // to 1e-7 degree
  if (data[3] == '5') step = 100;
  else if (data[3] == '4') step = 1000;
  else return false;
  const int64_t per_deg = 10000000 / step;
  const int64_t x0 = static_cast<int64_t>((cell % kCols) * kCellDeg - 180) * per_deg;
  const int64_t y0 = static_cast<int64_t>((cell / kCols) * kCellDeg - 90) * per_deg;

  Reader r{data + 4, data + len};
  while (r.ok && r.p < r.end) {
    Line ln;
    const uint8_t kind = r.byte();
    if (kind > static_cast<uint8_t>(Kind::Airspace)) {
      r.ok = false;
      break;
    }
    ln.kind = static_cast<Kind>(kind);
    if (ln.kind == Kind::Airspace) {
      const uint8_t cls = r.byte();
      if (cls > static_cast<uint8_t>(AirspaceClass::D)) {
        r.ok = false;
        break;
      }
      ln.cls = static_cast<AirspaceClass>(cls);
      ln.floor_100ft = static_cast<uint16_t>(std::min<uint64_t>(r.varint(), 65535));
      ln.ceil_100ft = static_cast<uint16_t>(std::min<uint64_t>(r.varint(), 65535));
    }
    const uint64_t n = r.varint();
    if (!r.ok || n < 2 || n > static_cast<uint64_t>(r.end - r.p)) {  // each point takes 2+ bytes
      r.ok = false;
      break;
    }
    ln.first = static_cast<uint32_t>(out->pts.size() / 2);
    ln.count = static_cast<uint32_t>(n);
    ln.lat_min = ln.lon_min = INT32_MAX;
    ln.lat_max = ln.lon_max = INT32_MIN;
    int64_t x = x0, y = y0;
    for (uint64_t i = 0; i < n && r.ok; ++i) {
      x += r.zigzag();
      y += r.zigzag();
      const int64_t lat = y * step, lon = x * step;
      if (lat < -900000000 || lat > 900000000 || lon < -1900000000 || lon > 1900000000) r.ok = false;
      const int32_t la = static_cast<int32_t>(lat), lo = static_cast<int32_t>(lon);
      out->pts.push_back(la);
      out->pts.push_back(lo);
      ln.lat_min = std::min(ln.lat_min, la);
      ln.lat_max = std::max(ln.lat_max, la);
      ln.lon_min = std::min(ln.lon_min, lo);
      ln.lon_max = std::max(ln.lon_max, lo);
    }
    out->lines.push_back(ln);
  }
  if (!r.ok) {
    out->lines.clear();
    out->pts.clear();
    return false;
  }
  return true;
}

bool decode(int cell, const uint8_t* data, size_t len, Shapes* out) {
  std::string raw;
  if (!core::inflateRaw(data, len, &raw, 4 * 1024 * 1024)) {
    out->lines.clear();
    out->pts.clear();
    return false;
  }
  return parse(cell, reinterpret_cast<const uint8_t*>(raw.data()), raw.size(), out);
}

}  // namespace core::geo
