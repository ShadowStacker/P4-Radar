#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

/**
 * Precipitation radar from RainViewer (free tier: past 2 hours in 10-minute
 * frames, tiles up to zoom 7, one color scheme). Parsing, tile URLs and
 * drawing are here; downloading is in app/weather, display in ui/weather_layer.
 */
namespace core::weather {

constexpr char kMapsUrl[] = "https://api.rainviewer.com/public/weather-maps.json";
constexpr int kMaxZoom = 7;
constexpr int kTileSize = 256;
constexpr char kAttribution[] = "Weather radar: RainViewer";

struct Frame {
  int64_t time = 0;  // unix seconds
  std::string path;  // "/v2/radar/<hash>"
};

struct Maps {
  std::string host;           // "https://tilecache.rainviewer.com"
  std::vector<Frame> frames;  // oldest first
};

/** Reads weather-maps.json (its radar.past frames). */
bool parseMaps(const char* json, size_t len, Maps* out);

/** Tile URL: universal blue colors (scheme 2), sharp edges, snow shown. */
std::string tileUrl(const Maps& m, const Frame& f, int z, uint32_t x, uint32_t y);

/** A decoded radar tile, 1 byte per pixel into a color table (index 0 = clear). */
struct Tile {
  uint8_t index[kTileSize * kTileSize];
  uint32_t palette[256];  // 0xAARRGGBB
};

/**
 * RGBA pixels -> Tile, with `opacity_pct` applied. Radar images have few
 * colors; past 255 distinct ones, the nearest existing color is used.
 */
void palettize(const uint8_t* rgba, int opacity_pct, Tile* out);

/** Zoom for tiles at this scale (about one tile pixel per screen pixel), at most kMaxZoom. */
int zoomFor(double km_per_px, double lat);

/** Packs a tile id: frame time, zoom, column, row. */
uint64_t key(int64_t time, int z, uint32_t x, uint32_t y);
void unpack(uint64_t key, int64_t* time, int* z, uint32_t* x, uint32_t* y);

/**
 * Draws one frame into a W x H ARGB8888 picture (cleared first), for a
 * north-up view where screen column c is longitude lon0 + c * dlon and row r
 * is latitude lat0 + r * dlat (dlat < 0). `tile(x, y)` gives the loaded tile
 * at zoom z, or null (left clear).
 */
void compose(uint32_t* out, int W, int H, double lon0, double dlon, double lat0, double dlat, int z,
             const std::function<const Tile*(uint32_t x, uint32_t y)>& tile, int opacity_pct = 100);

}  // namespace core::weather
