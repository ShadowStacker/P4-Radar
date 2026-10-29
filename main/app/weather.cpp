#include "app/weather.h"

#include <atomic>
#include <cstdio>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>

#include "core/png.h"
#include "platform/platform.h"

namespace app::weather {

namespace {

constexpr uint32_t kRefreshMs = 10 * 60 * 1000;  // RainViewer adds a frame every 10 minutes
constexpr uint32_t kRetryMs = 60 * 1000;
constexpr size_t kMaxReady = 8;

std::atomic<bool> s_enabled{false};
std::mutex s_mu;  // guards everything below
core::weather::Maps s_maps;
bool s_have_maps = false;
uint32_t s_next_maps_ms = 0;
std::vector<uint64_t> s_missing;
std::deque<std::pair<uint64_t, core::weather::Tile*>> s_ready;
std::unordered_map<uint64_t, uint32_t> s_failed;  // key -> retry after (ms); 0 = never

bool fetchMaps() {
  std::string body;
  const int code = plat::httpGetKeepAlive(core::weather::kMapsUrl, &body, 10000);
  core::weather::Maps m;
  const bool ok = code == 200 && core::weather::parseMaps(body.data(), body.size(), &m);
  std::printf("weather: frame list -> HTTP %d, %u frames\n", code, ok ? static_cast<unsigned>(m.frames.size()) : 0u);
  std::lock_guard<std::mutex> lock(s_mu);
  s_next_maps_ms = plat::millis() + (ok ? kRefreshMs : kRetryMs);
  if (ok) {
    s_maps = m;
    s_have_maps = true;
  }
  return true;
}

}  // namespace

void setEnabled(bool on) {
  if (s_enabled.exchange(on) == on || on) return;
  // Turned off: forget what's queued (decoded tiles are freed by their owner).
  std::lock_guard<std::mutex> lock(s_mu);
  s_missing.clear();
  for (auto& r : s_ready) delete r.second;
  s_ready.clear();
}

void request(const std::vector<uint64_t>& missing) {
  std::lock_guard<std::mutex> lock(s_mu);
  s_missing = missing;
}

bool popReady(uint64_t* key, core::weather::Tile** tile) {
  std::lock_guard<std::mutex> lock(s_mu);
  if (s_ready.empty()) return false;
  *key = s_ready.front().first;
  *tile = s_ready.front().second;
  s_ready.pop_front();
  return true;
}

std::vector<int64_t> frames() {
  std::lock_guard<std::mutex> lock(s_mu);
  std::vector<int64_t> out;
  for (const core::weather::Frame& f : s_maps.frames) out.push_back(f.time);
  return out;
}

bool service() {
  if (!s_enabled.load()) return false;
  uint64_t key = 0;
  std::string url;
  {
    std::lock_guard<std::mutex> lock(s_mu);
    const uint32_t now = plat::millis();
    if (static_cast<int32_t>(now - s_next_maps_ms) >= 0) {  // frame list due (or never fetched)
      s_next_maps_ms = now + kRetryMs;
    } else {
      if (s_ready.size() >= kMaxReady) return false;
      for (uint64_t k : s_missing) {
        auto f = s_failed.find(k);
        if (f != s_failed.end() && (f->second == 0 || static_cast<int32_t>(now - f->second) < 0)) continue;
        int64_t t;
        int z;
        uint32_t x, y;
        core::weather::unpack(k, &t, &z, &x, &y);
        for (const core::weather::Frame& fr : s_maps.frames) {
          if (fr.time == t) {
            url = core::weather::tileUrl(s_maps, fr, z, x, y);
            key = k;
            break;
          }
        }
        if (!url.empty()) break;
      }
      if (url.empty()) return false;
    }
  }
  if (url.empty()) return fetchMaps();

  std::string png;
  const int code = plat::httpGetKeepAlive(url.c_str(), &png, 10000);
  core::png::Image img;
  const bool ok = code == 200 && core::png::decode(reinterpret_cast<const uint8_t*>(png.data()), png.size(), &img) &&
                  img.width == core::weather::kTileSize && img.height == core::weather::kTileSize;
  if (!ok) {
    std::printf("weather: tile %s -> HTTP %d%s\n", url.c_str(), code, code == 200 ? " (not a 256 px PNG)" : "");
    std::lock_guard<std::mutex> lock(s_mu);
    if (s_failed.size() > 256) s_failed.clear();
    s_failed[key] = code == 404 || code == 200 ? 0 : plat::millis() + kRetryMs;
    return true;
  }
  auto* tile = new core::weather::Tile;
  core::weather::palettize(img.rgba.data(), 100, tile);
  std::lock_guard<std::mutex> lock(s_mu);
  s_ready.emplace_back(key, tile);
  return true;
}

}  // namespace app::weather
