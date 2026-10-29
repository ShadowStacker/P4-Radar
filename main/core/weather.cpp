#include "core/weather.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "cJSON.h"
#include "core/tiles.h"

namespace core::weather {

bool parseMaps(const char* json, size_t len, Maps* out) {
  *out = Maps{};
  cJSON* root = cJSON_ParseWithLength(json, len);
  if (root == nullptr) return false;
  const cJSON* host = cJSON_GetObjectItemCaseSensitive(root, "host");
  const cJSON* radar = cJSON_GetObjectItemCaseSensitive(root, "radar");
  const cJSON* past = radar ? cJSON_GetObjectItemCaseSensitive(radar, "past") : nullptr;
  if (cJSON_IsString(host)) out->host = host->valuestring;
  const cJSON* f = nullptr;
  cJSON_ArrayForEach(f, past) {
    const cJSON* t = cJSON_GetObjectItemCaseSensitive(f, "time");
    const cJSON* p = cJSON_GetObjectItemCaseSensitive(f, "path");
    if (cJSON_IsNumber(t) && cJSON_IsString(p)) out->frames.push_back({static_cast<int64_t>(t->valuedouble), p->valuestring});
  }
  cJSON_Delete(root);
  std::sort(out->frames.begin(), out->frames.end(), [](const Frame& a, const Frame& b) { return a.time < b.time; });
  return !out->host.empty() && !out->frames.empty();
}

std::string tileUrl(const Maps& m, const Frame& f, int z, uint32_t x, uint32_t y) {
  char tail[64];
  std::snprintf(tail, sizeof(tail), "/%d/%d/%u/%u/2/0_1.png", kTileSize, z, static_cast<unsigned>(x),
                static_cast<unsigned>(y));
  return m.host + f.path + tail;
}

void palettize(const uint8_t* rgba, int opacity_pct, Tile* out) {
  std::memset(out->palette, 0, sizeof(out->palette));
  int used = 1;  // 0 = clear
  uint32_t last_rgba = 0;
  uint8_t last_index = 0;
  for (int i = 0; i < kTileSize * kTileSize; ++i) {
    const uint8_t* p = rgba + i * 4;
    if (p[3] < 16) {
      out->index[i] = 0;
      continue;
    }
    const uint32_t c = static_cast<uint32_t>(p[0]) << 16 | static_cast<uint32_t>(p[1]) << 8 | p[2];
    const uint32_t a = static_cast<uint32_t>(p[3]) * opacity_pct / 100;
    const uint32_t argb = a << 24 | c;
    if (argb == last_rgba && last_index != 0) {  // runs of one color are the common case
      out->index[i] = last_index;
      continue;
    }
    int found = -1;
    for (int k = 1; k < used; ++k) {
      if (out->palette[k] == argb) {
        found = k;
        break;
      }
    }
    if (found < 0 && used < 256) {
      found = used++;
      out->palette[found] = argb;
    } else if (found < 0) {
      int best = 1, best_d = 1 << 30;  // full: nearest color
      for (int k = 1; k < 256; ++k) {
        const uint32_t q = out->palette[k];
        const int dr = static_cast<int>(q >> 16 & 255) - p[0], dg = static_cast<int>(q >> 8 & 255) - p[1],
                  db = static_cast<int>(q & 255) - p[2];
        const int d = dr * dr + dg * dg + db * db;
        if (d < best_d) {
          best_d = d;
          best = k;
        }
      }
      found = best;
    }
    out->index[i] = static_cast<uint8_t>(found);
    last_rgba = argb;
    last_index = static_cast<uint8_t>(found);
  }
}

int zoomFor(double km_per_px, double lat) {
  // Ground size of a tile pixel at zoom z: circumference * cos(lat) / (256 * 2^z).
  const double c = 40075.016686 * std::cos(lat * M_PI / 180.0) / kTileSize;
  const int z = static_cast<int>(std::lround(std::log2(c / std::max(km_per_px, 1e-6))));
  return std::min(std::max(z, 1), kMaxZoom);
}

uint64_t key(int64_t time, int z, uint32_t x, uint32_t y) {
  return static_cast<uint64_t>(time & 0xFFFFFFFFFF) << 24 | static_cast<uint64_t>(z & 15) << 20 |
         static_cast<uint64_t>(x & 1023) << 10 | (y & 1023);
}

void unpack(uint64_t k, int64_t* time, int* z, uint32_t* x, uint32_t* y) {
  *time = static_cast<int64_t>(k >> 24);
  *z = static_cast<int>(k >> 20 & 15);
  *x = static_cast<uint32_t>(k >> 10 & 1023);
  *y = static_cast<uint32_t>(k & 1023);
}

void compose(uint32_t* out, int W, int H, double lon0, double dlon, double lat0, double dlat, int z,
             const std::function<const Tile*(uint32_t x, uint32_t y)>& tile, int opacity_pct) {
  std::memset(out, 0, static_cast<size_t>(W) * H * 4);
  const double n = std::ldexp(1.0, z) * kTileSize;  // world width in tile pixels
  // Per column: which tile and which pixel in it.
  std::vector<int32_t> col_tile(W), col_px(W);
  for (int c = 0; c < W; ++c) {
    double lon = lon0 + c * dlon;
    lon = std::fmod(lon + 540.0, 360.0) - 180.0;
    const double gx = (lon + 180.0) / 360.0 * n;
    const int64_t g = static_cast<int64_t>(std::floor(gx));
    col_tile[c] = static_cast<int32_t>(g / kTileSize);
    col_px[c] = static_cast<int32_t>(g % kTileSize);
  }
  for (int r = 0; r < H; ++r) {
    const double lat = lat0 + r * dlat;
    if (lat > 85.05 || lat < -85.05) continue;
    const double s = std::sin(lat * M_PI / 180.0);
    const double gy = (0.5 - std::log((1 + s) / (1 - s)) / (4 * M_PI)) * n;
    const int64_t g = static_cast<int64_t>(std::floor(gy));
    const uint32_t ty = static_cast<uint32_t>(g / kTileSize);
    const int py = static_cast<int>(g % kTileSize);
    uint32_t* row = out + static_cast<size_t>(r) * W;
    int32_t cached_tx = -1;
    const Tile* t = nullptr;
    for (int c = 0; c < W; ++c) {
      if (col_tile[c] != cached_tx) {
        cached_tx = col_tile[c];
        t = tile(static_cast<uint32_t>(cached_tx), ty);
      }
      if (t == nullptr) continue;
      const uint8_t i = t->index[py * kTileSize + col_px[c]];
      if (i == 0) continue;
      const uint32_t argb = t->palette[i];
      row[c] = opacity_pct >= 100 ? argb : ((argb >> 24) * opacity_pct / 100) << 24 | (argb & 0xFFFFFF);
    }
  }
}

}  // namespace core::weather
