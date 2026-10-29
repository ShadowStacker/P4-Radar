#include "core/tiles.h"

#include <cmath>
#include <cstdio>

namespace core::tiles {

namespace {

constexpr double kMaxLat = 85.05112878;
constexpr double kEarthCircumferenceKm = 40075.016686;

}  // namespace

Variant variantFor(Style style, bool labels, bool have_carto_key) {
  switch (style) {
    case Style::DarkStreets:
      if (!have_carto_key) return Variant::OsmStandard;
      return labels ? Variant::DarkLabels : Variant::DarkNoLabels;
    case Style::Topo: return Variant::Topo;
    case Style::FaaSectional: return Variant::FaaSectional;
    case Style::Satellite: return Variant::Satellite;
    case Style::StreetMap: return Variant::StreetMap;
    default: return Variant::None;
  }
}

int maxZoom(Variant v) {
  switch (v) {
    case Variant::Topo: return 17;
    case Variant::FaaSectional: return 12;  // sectionals are 1:500,000 scans; closer is just enlarged
    case Variant::StreetMap: return 19;
    default: return 18;
  }
}

int minZoom(Variant v) { return v == Variant::FaaSectional ? 8 : 1; }

// Charts, photos and the street map are shown as they are: tinting would hide
// what they show (the street map would lose its colors and readable names).
bool keepColors(Variant v) {
  return v == Variant::FaaSectional || v == Variant::Satellite || v == Variant::StreetMap;
}

const char* dirName(Variant v) {
  switch (v) {
    // CARTO tiles saved before v1.10 ("dark", "darknl") carry an "API KEY
    // REQUIRED" watermark, so keyed tiles use new folders.
    case Variant::DarkLabels: return "cdark";
    case Variant::DarkNoLabels: return "cdarknl";
    case Variant::Topo: return "topo";
    case Variant::OsmStandard: return "osm";
    case Variant::FaaSectional: return "faavfr";
    case Variant::Satellite: return "esrisat";
    case Variant::StreetMap: return "osm";  // the same tiles as OsmStandard, only drawn differently
    default: return "none";
  }
}

// The built-in fonts have no (c) glyph, hence "(c)".
const char* attribution(Variant v) {
  switch (v) {
    case Variant::DarkLabels:
    case Variant::DarkNoLabels: return "(c) OpenStreetMap contributors (c) CARTO";
    case Variant::Topo: return "(c) OpenStreetMap contributors, SRTM | (c) OpenTopoMap (CC-BY-SA)";
    case Variant::OsmStandard: return "(c) OpenStreetMap contributors";
    case Variant::FaaSectional: return "FAA VFR Sectional - not for navigation";
    case Variant::Satellite: return "Esri, Maxar, Earthstar Geographics";
    case Variant::StreetMap: return "(c) OpenStreetMap contributors";
    default: return "";
  }
}

// Light maps get dark backings behind aircraft labels (photos too: they're busy).
bool isLight(Variant v) {
  return v == Variant::Topo || v == Variant::OsmStandard || v == Variant::FaaSectional || v == Variant::Satellite ||
         v == Variant::StreetMap;
}

std::string url(Variant v, int z, uint32_t x, uint32_t y, int alternative, const char* key) {
  char buf[192];
  switch (v) {
    case Variant::DarkLabels:
    case Variant::DarkNoLabels: {
      const char* name = v == Variant::DarkLabels ? "dark_all" : "dark_nolabels";
      std::snprintf(buf, sizeof(buf), "https://a.basemaps.cartocdn.com/%s%s/%d/%u/%u.png%s%s",
                    alternative ? "rastertiles/" : "", name, z, static_cast<unsigned>(x), static_cast<unsigned>(y),
                    key && key[0] ? "?key=" : "", key ? key : "");
      break;
    }
    case Variant::Topo:
      std::snprintf(buf, sizeof(buf), "https://%s.tile.opentopomap.org/%d/%u/%u.png", alternative ? "b" : "a", z,
                    static_cast<unsigned>(x), static_cast<unsigned>(y));
      break;
    case Variant::OsmStandard:
    case Variant::StreetMap:
      std::snprintf(buf, sizeof(buf), "https://tile.openstreetmap.org/%d/%u/%u.png", z, static_cast<unsigned>(x),
                    static_cast<unsigned>(y));
      break;
    case Variant::Satellite:
      // Esri World Imagery (JPEG). ArcGIS orders the path z/row/column.
      std::snprintf(buf, sizeof(buf),
                    "https://%s.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/%d/%u/%u",
                    alternative ? "services" : "server", z, static_cast<unsigned>(y), static_cast<unsigned>(x));
      break;
    case Variant::FaaSectional:
      // The FAA's own ArcGIS tile service (Aeronautical Information Services);
      // ArcGIS orders the path z/row/column. Saved tiles keep the .png name
      // whatever the image format; the decoder looks at the bytes.
      std::snprintf(buf, sizeof(buf),
                    "https://tiles.arcgis.com/tiles/ssFJjBXIUyZDrSYZ/arcgis/rest/services/VFR_Sectional/MapServer/"
                    "tile/%d/%u/%u",
                    z, static_cast<unsigned>(y), static_cast<unsigned>(x));
      break;
    default: return "";
  }
  return buf;
}

std::string sdPath(const std::string& root, Variant v, int z, uint32_t x, uint32_t y) {
  char buf[96];
  std::snprintf(buf, sizeof(buf), "/tiles/%s/%d/%u/%u.png", dirName(v), z, static_cast<unsigned>(x),
                static_cast<unsigned>(y));
  return root + buf;
}

// key: variant 4 bits | z 5 bits | x 27 bits | y 27 bits (enough for z <= 26)
uint64_t key(Variant v, int z, uint32_t x, uint32_t y) {
  return (uint64_t(static_cast<uint8_t>(v)) << 59) | (uint64_t(z & 31) << 54) | (uint64_t(x & 0x7FFFFFF) << 27) |
         uint64_t(y & 0x7FFFFFF);
}

void unpack(uint64_t k, Variant* v, int* z, uint32_t* x, uint32_t* y) {
  *v = static_cast<Variant>((k >> 59) & 15);
  *z = static_cast<int>((k >> 54) & 31);
  *x = static_cast<uint32_t>((k >> 27) & 0x7FFFFFF);
  *y = static_cast<uint32_t>(k & 0x7FFFFFF);
}

uint64_t parentKey(uint64_t k, int levels) {
  Variant v;
  int z;
  uint32_t x, y;
  unpack(k, &v, &z, &x, &y);
  if (levels > z) levels = z;
  return key(v, z - levels, x >> levels, y >> levels);
}

int zoomFor(Variant v, double km_per_px, double lat) {
  // Ground size of one tile pixel at zoom 0 is circumference * cos(lat) / 256.
  const double px0 = kEarthCircumferenceKm * std::cos(degToRad(lat)) / kTileSize;
  int z = static_cast<int>(std::lround(std::log2(px0 / km_per_px)));
  if (z < minZoom(v)) z = minZoom(v);
  if (z > maxZoom(v)) z = maxZoom(v);
  return z;
}

uint32_t tileX(double lon, int z) {
  const double n = std::ldexp(1.0, z);
  double x = std::floor((lon + 180.0) / 360.0 * n);
  if (x < 0) x = 0;
  if (x > n - 1) x = n - 1;
  return static_cast<uint32_t>(x);
}

uint32_t tileY(double lat, int z) {
  if (lat > kMaxLat) lat = kMaxLat;
  if (lat < -kMaxLat) lat = -kMaxLat;
  const double n = std::ldexp(1.0, z);
  const double r = degToRad(lat);
  double y = std::floor((1.0 - std::log(std::tan(r) + 1.0 / std::cos(r)) / kPi) / 2.0 * n);
  if (y < 0) y = 0;
  if (y > n - 1) y = n - 1;
  return static_cast<uint32_t>(y);
}

double tileLon(uint32_t x, int z) { return x / std::ldexp(1.0, z) * 360.0 - 180.0; }

double tileLat(double y, int z) {
  const double n = kPi - 2.0 * kPi * y / std::ldexp(1.0, z);
  return radToDeg(std::atan(std::sinh(n)));
}

void recolor(const uint8_t* rgba, int pixels, bool light_map, bool tint, uint8_t tr, uint8_t tg, uint8_t tb,
             int brightness, uint16_t* out) {
  for (int i = 0; i < pixels; ++i, rgba += 4) {
    int r, g, b;
    if (tint) {
      int l = (rgba[0] * 54 + rgba[1] * 183 + rgba[2] * 19) >> 8;
      if (light_map) l = 255 - l;
      l = (l - 12) * 2;
      l = l < 0 ? 0 : (l > 255 ? 255 : l);
      l = l * brightness / 100;
      r = tr * l / 255;
      g = tg * l / 255;
      b = tb * l / 255;
    } else {
      r = rgba[0] * brightness / 100;
      g = rgba[1] * brightness / 100;
      b = rgba[2] * brightness / 100;
    }
    // Transparent pixels (rare in tiles) go to black.
    r = r * rgba[3] / 255;
    g = g * rgba[3] / 255;
    b = b * rgba[3] / 255;
    out[i] = static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
  }
}

}  // namespace core::tiles
