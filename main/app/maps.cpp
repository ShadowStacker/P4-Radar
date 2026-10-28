#include "app/maps.h"

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <deque>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>

#include "app/app.h"
#include "core/jpeg.h"
#include "core/png.h"
#include "platform/platform.h"

namespace app::maps {

namespace {

using core::tiles::Variant;

constexpr size_t kRamCacheBytes = 4 * 1024 * 1024;  // compressed PNGs
constexpr size_t kMaxReady = 12;                    // decoded tiles waiting for the UI
constexpr uint32_t kRetryMs = 60000;                // after a failed download
constexpr size_t kMaxTileBytes = 512 * 1024;

std::mutex s_mu;  // guards everything below except the RAM cache (network task only)
Look s_look;
uint32_t s_gen = 0;
std::vector<uint64_t> s_missing;
struct Ready {
  uint64_t key;
  uint32_t gen;
  uint16_t* pixels;
};
std::deque<Ready> s_ready;
std::unordered_map<uint64_t, uint32_t> s_failed;  // key -> when (ms); 404s never retried
std::unordered_map<uint64_t, bool> s_not_found;
// Both only grow, in internal RAM (small allocations land there); start over
// past this many so days of panning (or a long time offline) can't eat it.
constexpr size_t kMaxFailedTiles = 512;

void noteFailed(uint64_t key, bool permanent) {
  if (s_failed.size() >= kMaxFailedTiles) s_failed.clear();
  if (s_not_found.size() >= kMaxFailedTiles) s_not_found.clear();
  if (permanent) s_not_found[key] = true;
  else s_failed[key] = plat::millis();
}

SdStatus s_sd;
std::string s_sd_key;  // from carto_key.txt on the card
std::deque<SdAction> s_sd_actions;
bool s_sd_tried = false;

// RAM cache of compressed tiles (network task only): LRU.
std::list<uint64_t> s_lru;
std::unordered_map<uint64_t, std::pair<std::string, std::list<uint64_t>::iterator>> s_ram;
size_t s_ram_bytes = 0;

void ramPut(uint64_t key, const std::string& png) {
  auto it = s_ram.find(key);
  if (it != s_ram.end()) return;
  s_lru.push_front(key);
  s_ram.emplace(key, std::make_pair(png, s_lru.begin()));
  s_ram_bytes += png.size();
  while (s_ram_bytes > kRamCacheBytes && !s_lru.empty()) {
    const uint64_t old = s_lru.back();
    s_lru.pop_back();
    auto o = s_ram.find(old);
    if (o != s_ram.end()) {
      s_ram_bytes -= o->second.first.size();
      s_ram.erase(o);
    }
  }
}

const std::string* ramGet(uint64_t key) {
  auto it = s_ram.find(key);
  if (it == s_ram.end()) return nullptr;
  s_lru.splice(s_lru.begin(), s_lru, it->second.second);
  return &it->second.first;
}

bool sdUsable() {
  std::lock_guard<std::mutex> lock(s_mu);
  return s_sd.state == SdState::Mounted && !s_sd.busy && app::settings().sd_cache;
}

bool readFile(const std::string& path, std::string* out) {
  FILE* f = std::fopen(path.c_str(), "rb");
  if (f == nullptr) return false;
  std::fseek(f, 0, SEEK_END);
  const long n = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  bool ok = n > 0 && static_cast<size_t>(n) <= kMaxTileBytes;
  if (ok) {
    out->resize(static_cast<size_t>(n));
    ok = std::fread(&(*out)[0], 1, out->size(), f) == out->size();
  }
  std::fclose(f);
  return ok;
}

/** mkdir -p for the directories above `path`. */
void makeParents(const std::string& path) {
  for (size_t i = 1; i < path.size(); ++i) {
    if (path[i] != '/') continue;
    const std::string dir = path.substr(0, i);
    mkdir(dir.c_str(), 0775);  // EEXIST is fine
  }
}

void writeFile(const std::string& path, const std::string& data) {
  makeParents(path);
  // Write to "<y>.tmp" then rename, so a power cut never leaves half a tile.
  const std::string tmp = path.substr(0, path.size() - 4) + ".tmp";
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

/** Walks a directory tree: counts files and bytes, or deletes everything. */
void walk(const std::string& dir, bool remove_all, uint32_t* files, uint64_t* bytes) {
  DIR* d = opendir(dir.c_str());
  if (d == nullptr) return;
  while (dirent* e = readdir(d)) {
    if (std::strcmp(e->d_name, ".") == 0 || std::strcmp(e->d_name, "..") == 0) continue;
    const std::string p = dir + "/" + e->d_name;
    struct stat st;
    if (stat(p.c_str(), &st) != 0) continue;
    if (S_ISDIR(st.st_mode)) {
      walk(p, remove_all, files, bytes);
      if (remove_all) rmdir(p.c_str());
    } else {
      ++*files;
      *bytes += static_cast<uint64_t>(st.st_size);
      if (remove_all) unlink(p.c_str());
    }
  }
  closedir(d);
}

/** Reads carto_key.txt from the card root: first line, spaces trimmed. */
void loadSdKey() {
  std::string text;
  FILE* f = std::fopen((std::string(plat::sdRoot()) + "/carto_key.txt").c_str(), "rb");
  if (f != nullptr) {
    char buf[128] = "";
    if (std::fgets(buf, sizeof(buf), f)) text = buf;
    std::fclose(f);
  }
  std::string key;
  for (char c : text) {
    if (c == '\r' || c == '\n') break;
    if (c != ' ' && c != '\t') key.push_back(c);
  }
  std::lock_guard<std::mutex> lock(s_mu);
  s_sd_key = key;
}

void setSdFromPlatform(const plat::SdInfo& info) {
  std::lock_guard<std::mutex> lock(s_mu);
  s_sd.total_bytes = info.total_bytes;
  s_sd.free_bytes = info.free_bytes;
  std::snprintf(s_sd.error, sizeof(s_sd.error), "%s", info.error);
  if (info.mounted) {
    s_sd.state = SdState::Mounted;
  } else {
    s_sd.state = std::strcmp(info.error, "Unmounted") == 0 ? SdState::Unmounted : SdState::NoCard;
    s_sd.counted = false;
  }
}

}  // namespace

void recolor(const uint8_t* rgba, int pixels, const Look& look, uint16_t* out) {
  core::tiles::recolor(rgba, pixels, core::tiles::isLight(look.variant), look.tint, look.tint_r, look.tint_g,
                       look.tint_b, look.brightness, out);
}

void request(const Look& look, const std::vector<uint64_t>& missing) {
  std::lock_guard<std::mutex> lock(s_mu);
  if (look != s_look) {
    s_look = look;
    ++s_gen;
    for (Ready& r : s_ready) plat::freeLarge(r.pixels);
    s_ready.clear();
  }
  s_missing = missing;
}

bool popReady(uint64_t* key, uint16_t** pixels) {
  std::lock_guard<std::mutex> lock(s_mu);
  while (!s_ready.empty()) {
    Ready r = s_ready.front();
    s_ready.pop_front();
    if (r.gen != s_gen) {
      plat::freeLarge(r.pixels);
      continue;
    }
    *key = r.key;
    *pixels = r.pixels;
    return true;
  }
  return false;
}

bool service(bool online) {
  uint64_t key = 0;
  Look look;
  uint32_t gen;
  {
    std::lock_guard<std::mutex> lock(s_mu);
    if (s_look.variant == Variant::None || s_ready.size() >= kMaxReady) return false;
    const uint32_t now = plat::millis();
    bool found = false;
    for (uint64_t k : s_missing) {
      if (s_not_found.count(k)) continue;
      auto f = s_failed.find(k);
      if (f != s_failed.end() && now - f->second < kRetryMs) continue;
      bool queued = false;
      for (const Ready& r : s_ready) queued |= r.key == k && r.gen == s_gen;
      if (queued) continue;
      key = k;
      found = true;
      break;
    }
    if (!found) return false;
    look = s_look;
    gen = s_gen;
  }

  Variant v;
  int z;
  uint32_t x, y;
  core::tiles::unpack(key, &v, &z, &x, &y);

  // 1. RAM, 2. SD card, 3. internet.
  std::string png;
  bool from_net = false;
  if (const std::string* cached = ramGet(key)) {
    png = *cached;
  } else if (sdUsable() && readFile(core::tiles::sdPath(plat::sdRoot(), v, z, x, y), &png)) {
    ramPut(key, png);
  } else if (online) {
    int code = -1;
    for (int alt = 0; alt < 2; ++alt) {
      png.clear();
      code = plat::httpGetKeepAlive(core::tiles::url(v, z, x, y, alt, cartoKey().c_str()).c_str(), &png, 10000);
      if (code == 200) break;
    }
    if (code != 200 || png.empty()) {
      std::lock_guard<std::mutex> lock(s_mu);
      noteFailed(key, code == 404);
      std::printf("map: tile %d/%u/%u -> HTTP %d\n", z, static_cast<unsigned>(x), static_cast<unsigned>(y), code);
      return true;
    }
    from_net = true;
    ramPut(key, png);
  } else {
    // Offline and not cached: try again later.
    std::lock_guard<std::mutex> lock(s_mu);
    noteFailed(key, false);
    return false;
  }

  core::png::Image img;
  const auto* bytes = reinterpret_cast<const uint8_t*>(png.data());
  const bool decoded = core::jpeg::isJpeg(bytes, png.size()) ? core::jpeg::decode(bytes, png.size(), &img)
                                                             : core::png::decode(bytes, png.size(), &img);
  if (!decoded ||
      img.width != core::tiles::kTileSize || img.height != core::tiles::kTileSize) {
    std::lock_guard<std::mutex> lock(s_mu);
    noteFailed(key, true);  // not a tile we can use
    std::printf("map: tile %d/%u/%u is not a 256 px PNG or JPEG\n", z, static_cast<unsigned>(x), static_cast<unsigned>(y));
    return true;
  }
  if (from_net && sdUsable()) writeFile(core::tiles::sdPath(plat::sdRoot(), v, z, x, y), png);

  const int n = core::tiles::kTileSize * core::tiles::kTileSize;
  auto* pixels = static_cast<uint16_t*>(plat::allocLarge(n * sizeof(uint16_t)));
  if (pixels == nullptr) return true;
  recolor(img.rgba.data(), n, look, pixels);

  std::lock_guard<std::mutex> lock(s_mu);
  if (gen != s_gen) {
    plat::freeLarge(pixels);  // the look changed while we worked
    return true;
  }
  s_ready.push_back({key, gen, pixels});
  return true;
}

std::string cartoKey() {
  const char* k = app::settings().carto_key;
  if (k[0] != '\0') return k;
  std::lock_guard<std::mutex> lock(s_mu);
  return s_sd_key;
}

bool hasCartoKey() { return !cartoKey().empty(); }

const char* cartoKeySource() {
  if (app::settings().carto_key[0] != '\0') return "settings";
  std::lock_guard<std::mutex> lock(s_mu);
  return s_sd_key.empty() ? "" : "SD card";
}

SdStatus sdStatus() {
  std::lock_guard<std::mutex> lock(s_mu);
  return s_sd;
}

void sdRequest(SdAction action) {
  std::lock_guard<std::mutex> lock(s_mu);
  s_sd_actions.push_back(action);
  if (action == SdAction::Count || action == SdAction::Clear) s_sd.busy = true;
}

bool serviceSd() {
  SdAction action;
  {
    std::lock_guard<std::mutex> lock(s_mu);
    if (!s_sd_tried) {
      s_sd_tried = true;
      action = SdAction::Mount;  // first call: look for a card
    } else if (!s_sd_actions.empty()) {
      action = s_sd_actions.front();
      s_sd_actions.pop_front();
    } else {
      return false;
    }
  }
  const std::string tiles_dir = std::string(plat::sdRoot()) + "/tiles";
  switch (action) {
    case SdAction::Mount: {
      plat::SdInfo info;
      plat::sdMount(&info);
      setSdFromPlatform(info);
      if (info.mounted) {
        loadSdKey();
        app::sdFilesMounted();  // settings.txt and favorites.txt
      }
      break;
    }
    case SdAction::Unmount:
      plat::sdUnmount();
      setSdFromPlatform(plat::sdInfo());
      break;
    case SdAction::Count:
    case SdAction::Clear: {
      uint32_t files = 0;
      uint64_t bytes = 0;
      const bool mounted = plat::sdInfo().mounted;
      if (mounted) walk(tiles_dir, action == SdAction::Clear, &files, &bytes);
      setSdFromPlatform(plat::sdInfo());  // free space changed
      std::lock_guard<std::mutex> lock(s_mu);
      s_sd.busy = false;
      s_sd.counted = mounted;
      s_sd.tiles = action == SdAction::Clear ? 0 : files;
      s_sd.tile_bytes = action == SdAction::Clear ? 0 : bytes;
      break;
    }
  }
  return true;
}

}  // namespace app::maps
