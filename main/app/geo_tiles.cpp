#include "app/geo_tiles.h"

#include <sys/stat.h>

#include <cstdio>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <string>

#include "app/app.h"
#include "app/maps.h"
#include "data/geo_world.h"
#include "platform/platform.h"

namespace app::geo {

namespace {

using Ptr = std::shared_ptr<const core::geo::Shapes>;

constexpr size_t kMaxDetailBytes = 6 * 1024 * 1024;  // decoded detailed cells kept in RAM (PSRAM)
constexpr uint32_t kRetryMs = 60 * 1000;             // after a failed download
constexpr size_t kMaxFileBytes = 512 * 1024;

std::mutex s_mu;
std::vector<int> s_wanted;
std::map<int, Ptr> s_detail;       // loaded detailed cells
std::list<int> s_lru;              // most recently drawn first
size_t s_detail_bytes = 0;
std::map<int, uint32_t> s_failed;  // cell -> when (millis)
std::set<int> s_not_found;
std::map<int, Ptr> s_coarse;       // decoded built-in cells (small; kept)
uint32_t s_gen = 0;

void touch(int cell) {
  s_lru.remove(cell);
  s_lru.push_front(cell);
}

void evict() {
  while (s_detail_bytes > kMaxDetailBytes && !s_lru.empty()) {
    const int victim = s_lru.back();
    s_lru.pop_back();
    auto it = s_detail.find(victim);
    if (it == s_detail.end()) continue;
    s_detail_bytes -= it->second->bytes();
    s_detail.erase(it);
  }
}

bool readFile(const std::string& path, std::string* out) {
  FILE* f = std::fopen(path.c_str(), "rb");
  if (f == nullptr) return false;
  std::fseek(f, 0, SEEK_END);
  const long n = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  bool ok = n > 0 && static_cast<size_t>(n) <= kMaxFileBytes;
  if (ok) {
    out->resize(static_cast<size_t>(n));
    ok = std::fread(&(*out)[0], 1, out->size(), f) == out->size();
  }
  std::fclose(f);
  return ok;
}

void writeFile(const std::string& path, const std::string& data) {
  for (size_t i = 1; i < path.size(); ++i) {  // mkdir -p
    if (path[i] == '/') mkdir(path.substr(0, i).c_str(), 0775);
  }
  const std::string tmp = path + ".tmp";  // a power cut never leaves half a file
  FILE* f = std::fopen(tmp.c_str(), "wb");
  if (f == nullptr) return;
  const bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
  std::fclose(f);
  if (ok) {
    std::remove(path.c_str());
    std::rename(tmp.c_str(), path.c_str());
  } else {
    std::remove(tmp.c_str());
  }
}

bool sdUsable() {
  const maps::SdStatus sd = maps::sdStatus();
  return sd.state == maps::SdState::Mounted && !sd.busy && app::settings().sd_cache;
}

}  // namespace

bool hasDetail(int cell) {
  return cell >= 0 && cell < core::geo::kCells && (data::kGeoDetailCells[cell >> 3] >> (cell & 7)) & 1;
}

std::string url(int cell) {
  return "https://raw.githubusercontent.com/ShadowStacker/P4-Radar/main/geo/v1/" + std::to_string(cell) + ".bin";
}

std::string sdPath(int cell) { return std::string(plat::sdRoot()) + "/geo/v1/" + std::to_string(cell) + ".bin"; }

void want(const std::vector<int>& cells) {
  std::lock_guard<std::mutex> lock(s_mu);
  s_wanted = cells;
}

std::shared_ptr<const core::geo::Shapes> shapes(int cell, bool* detailed) {
  if (detailed) *detailed = false;
  if (cell < 0 || cell >= core::geo::kCells) return nullptr;
  {
    std::lock_guard<std::mutex> lock(s_mu);
    auto it = s_detail.find(cell);
    if (it != s_detail.end()) {
      touch(cell);
      if (detailed) *detailed = true;
      return it->second;
    }
    auto c = s_coarse.find(cell);
    if (c != s_coarse.end()) return c->second;
  }
  // The built-in coarse cell, decoded once (UI task only, so no race on the insert).
  const uint32_t a = data::kGeoWorldOffset[cell], b = data::kGeoWorldOffset[cell + 1];
  Ptr p;
  if (b > a) {
    auto s = std::make_shared<core::geo::Shapes>();
    if (core::geo::decode(cell, data::kGeoWorld + a, b - a, s.get())) p = s;
  }
  std::lock_guard<std::mutex> lock(s_mu);
  s_coarse[cell] = p;
  return p;
}

uint32_t generation() {
  std::lock_guard<std::mutex> lock(s_mu);
  return s_gen;
}

bool service(bool online) {
  int cell = -1;
  {
    std::lock_guard<std::mutex> lock(s_mu);
    const uint32_t now = plat::millis();
    for (int c : s_wanted) {
      if (!hasDetail(c) || s_detail.count(c) || s_not_found.count(c)) continue;
      auto f = s_failed.find(c);
      if (f != s_failed.end() && now - f->second < kRetryMs) continue;
      cell = c;
      break;
    }
  }
  if (cell < 0) return false;

  std::string body;
  bool from_net = false;
  const bool sd = sdUsable();
  if (!(sd && readFile(sdPath(cell), &body))) {
    if (!online) {
      std::lock_guard<std::mutex> lock(s_mu);
      s_failed[cell] = plat::millis();
      return false;
    }
    const int code = plat::httpGetKeepAlive(url(cell).c_str(), &body, 10000);
    if (code != 200 || body.empty() || body.size() > kMaxFileBytes) {
      std::printf("geo: cell %d -> HTTP %d\n", cell, code);
      std::lock_guard<std::mutex> lock(s_mu);
      if (code == 404) s_not_found.insert(cell);
      else s_failed[cell] = plat::millis();
      return true;
    }
    from_net = true;
  }
  auto shapes = std::make_shared<core::geo::Shapes>();
  if (!core::geo::decode(cell, reinterpret_cast<const uint8_t*>(body.data()), body.size(), shapes.get())) {
    std::printf("geo: cell %d is damaged%s\n", cell, from_net ? "" : " (on the SD card; fetching it again)");
    if (!from_net) std::remove(sdPath(cell).c_str());
    std::lock_guard<std::mutex> lock(s_mu);
    s_failed[cell] = from_net ? plat::millis() : plat::millis() - kRetryMs;  // a bad SD copy: download right away
    return true;
  }
  if (from_net && sd) {
    const std::string path = sdPath(cell);
    app::queueCardJob([path, body] { writeFile(path, body); }, body.size(), true);
  }
  std::lock_guard<std::mutex> lock(s_mu);
  s_detail_bytes += shapes->bytes();
  s_detail[cell] = shapes;
  touch(cell);
  s_failed.erase(cell);
  evict();
  ++s_gen;
  return true;
}

}  // namespace app::geo
