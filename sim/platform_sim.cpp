// Fake implementations of platform.h for the desktop simulator.

#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <thread>

#include <zlib.h>

#include "app/app.h"
#include "platform/platform.h"
#include "sim_state.h"

namespace sim {
uint32_t g_ms = 1000;
Touch g_touch;
bool g_preset_wifi = true;
bool g_update_available = false;
std::string g_out_dir = "sim_out";
std::function<std::string(const std::string&)> g_trace_server;
std::function<std::string(const std::string&)> g_route_server;
}  // namespace sim

namespace plat {

namespace {
core::Settings s_saved;
bool s_have_saved = false;
bool s_scan_pending = false;
int s_volume = 60;
std::set<int> s_rendered_sounds;

const char* soundName(core::Sound s) {
  switch (s) {
    case core::Sound::Click: return "click";
    case core::Sound::Ping: return "ping";
    case core::Sound::NewAircraft: return "new_aircraft";
    case core::Sound::Alert: return "alert";
    case core::Sound::Emergency: return "emergency";
    case core::Sound::Startup: return "startup";
    case core::Sound::Home: return "home";
    case core::Sound::FavoriteSeen: return "favorite_detected";
    case core::Sound::FavoriteNear: return "favorite_nearby";
    case core::Sound::Alarm: return "alarm";
    default: return "unknown";
  }
}

void writeWav(const std::string& path, const std::vector<int16_t>& pcm, int rate) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (f == nullptr) return;
  const uint32_t data_bytes = static_cast<uint32_t>(pcm.size() * 2);
  auto u32 = [f](uint32_t v) { std::fwrite(&v, 4, 1, f); };
  auto u16 = [f](uint16_t v) { std::fwrite(&v, 2, 1, f); };
  std::fwrite("RIFF", 1, 4, f);
  u32(36 + data_bytes);
  std::fwrite("WAVEfmt ", 1, 8, f);
  u32(16);
  u16(1);
  u16(1);
  u32(rate);
  u32(rate * 2);
  u16(2);
  u16(16);
  std::fwrite("data", 1, 4, f);
  u32(data_bytes);
  std::fwrite(pcm.data(), 2, pcm.size(), f);
  std::fclose(f);
}
}  // namespace

uint32_t millis() { return sim::g_ms; }

void settingsLoad(core::Settings* s) {
  if (s_have_saved) {
    *s = s_saved;
    return;
  }
  *s = core::Settings{};
  if (sim::g_preset_wifi) {
    std::snprintf(s->wifi_ssid, sizeof(s->wifi_ssid), "HomeNet");
    std::snprintf(s->wifi_pass, sizeof(s->wifi_pass), "hunter22");
  }
}

void settingsSave(const core::Settings& s) {
  s_saved = s;
  s_have_saved = true;
}

void settingsErase() { s_have_saved = false; }

namespace {
std::vector<core::Favorite> s_favorites;
}
void favoritesLoad(std::vector<core::Favorite>* out) { *out = s_favorites; }
void favoritesSave(const std::vector<core::Favorite>& favs) { s_favorites = favs; }

WifiStatus wifiStatus() {
  WifiStatus ws;
  const char* ssid = app::settings().wifi_ssid;
  if (ssid[0] == '\0') return ws;
  ws.state = WifiState::Connected;
  std::snprintf(ws.ssid, sizeof(ws.ssid), "%s", ssid);
  std::snprintf(ws.ip, sizeof(ws.ip), "192.168.1.42");
  ws.rssi = -54;
  return ws;
}

void wifiConnect(const char*, const char*) {}
void wifiScanStart() { s_scan_pending = true; }

bool wifiScanResults(std::vector<WifiNetwork>* out) {
  if (!s_scan_pending) return false;
  s_scan_pending = false;
  *out = {{"HomeNet", -48, true}, {"HomeNet-5G", -57, true}, {"Neighbours", -71, true},
          {"CoffeeShop Guest", -79, false}, {"PrinterDirect-4F2A", -84, true}};
  return true;
}

// Fake map tiles: procedural "streets" (dark) or "topo" (light) PNGs whose
// features line up across tiles. Served as palette/RGB PNGs like the real ones.
std::string pngChunkSim(const char* type, const std::string& data) {
  std::string out;
  const uint32_t n = static_cast<uint32_t>(data.size());
  for (int i = 3; i >= 0; --i) out.push_back(static_cast<char>(n >> (i * 8)));
  const std::string td = std::string(type, 4) + data;
  out += td;
  const uLong crc = crc32(0, reinterpret_cast<const Bytef*>(td.data()), static_cast<uInt>(td.size()));
  for (int i = 3; i >= 0; --i) out.push_back(static_cast<char>(crc >> (i * 8)));
  return out;
}

std::string fakeTile(bool topo, int z, uint32_t tx, uint32_t ty, bool osm = false, bool faa = false) {
  const int T = 256;
  std::string raw;
  raw.reserve((T * 3 + 1) * T);
  // World pixel coordinates scaled to a zoom-independent unit, so features
  // keep their place on the ground at every zoom.
  const double scale = std::ldexp(1.0, 14 - z);
  for (int y = 0; y < T; ++y) {
    raw.push_back(0);
    for (int x = 0; x < T; ++x) {
      const double gx = (tx * T + x) * scale, gy = (ty * T + y) * scale;
      const double px = scale;  // one screen pixel in world units (line width)
      int r, g, b;
      if (faa) {
        // Sectional-ish: tan terrain tints, blue Class B rings, magenta airways.
        r = 230; g = 226; b = 205;
        if (std::sin(gx / 1700.0) * std::cos(gy / 1500.0) > 0.35) { r = 214; g = 222; b = 190; }
        const double d = std::hypot(std::fmod(gx, 9000.0) - 4500.0, std::fmod(gy, 9000.0) - 4500.0);
        if (std::fabs(std::fmod(d, 900.0) - 450.0) < 2.5 * px) { r = 40; g = 90; b = 190; }
        if (std::fmod(std::fabs(gx - 0.6 * gy), 4100.0) < 2.5 * px) { r = 170; g = 60; b = 150; }
      } else if (osm) {
        r = 242; g = 239; b = 233;                                                           // land
        if (std::sin(gx / 900.0) + std::cos(gy / 700.0) > 1.3) { r = 170; g = 211; b = 223; }  // water
        if (std::fmod(gx, 410.0) < 1.5 * px || std::fmod(gy + 0.2 * gx, 530.0) < 1.5 * px) { r = g = b = 255; }
        if (std::fmod(gx + 0.37 * gy, 3100.0) < 3 * px || std::fmod(gy, 2300.0) < 3 * px) { r = 248; g = 178; b = 156; }
      } else if (!topo) {
        r = g = 14; b = 18;
        if (std::sin(gx / 900.0) + std::cos(gy / 700.0) > 1.3) { r = 20; g = 26; b = 40; }  // water
        const bool major = std::fmod(gx + 0.37 * gy, 3100.0) < 2.5 * px || std::fmod(gy, 2300.0) < 2.5 * px;
        const bool minor = std::fmod(gx, 410.0) < 1.2 * px || std::fmod(gy + 0.2 * gx, 530.0) < 1.2 * px;
        if (minor && z >= 11) { r = g = 48; b = 52; }
        if (major) { r = g = 96; b = 100; }
      } else {
        r = 242; g = 239; b = 230;
        const double hgt = 300 * std::sin(gx / 2600.0) + 260 * std::cos(gy / 2100.0) + gx / 900.0;
        if (std::sin(gx / 1500.0) * std::cos(gy / 1300.0) > 0.45) { r = 205; g = 228; b = 190; }  // forest
        if (std::fmod(hgt + 10000.0, 40.0) < 2.5) { r = 190; g = 160; b = 125; }                  // contours
        if (std::fmod(gx + 0.5 * gy, 2900.0) < 2.0 * px) { r = 225; g = 120; b = 70; }           // roads
      }
      raw.push_back(static_cast<char>(r));
      raw.push_back(static_cast<char>(g));
      raw.push_back(static_cast<char>(b));
    }
  }
  std::string zbuf(compressBound(raw.size()), '\0');
  uLongf zn = zbuf.size();
  compress2(reinterpret_cast<Bytef*>(&zbuf[0]), &zn, reinterpret_cast<const Bytef*>(raw.data()), raw.size(), 1);
  zbuf.resize(zn);
  std::string ihdr = {0, 0, 1, 0, 0, 0, 1, 0, 8, 2, 0, 0, 0};  // 256x256, 8-bit RGB
  return std::string("\x89PNG\r\n\x1a\n", 8) + pngChunkSim("IHDR", ihdr) + pngChunkSim("IDAT", zbuf) +
         pngChunkSim("IEND", "");
}

// Only the trace endpoint is simulated (traffic is fed directly). Bodies are
// gzip-compressed like the real servers send them.
int httpGet(const char* url, std::string* body, int, const char* const*) {
  const bool carto = std::strstr(url, "basemaps.cartocdn.com/") != nullptr;
  const bool topo = std::strstr(url, "tile.opentopomap.org/") != nullptr;
  const bool osm = std::strstr(url, "tile.openstreetmap.org/") != nullptr;
  const bool faa = std::strstr(url, "VFR_Sectional/MapServer/tile/") != nullptr;
  const bool sat = std::strstr(url, "World_Imagery/MapServer/tile/") != nullptr;
  if (carto || topo || osm || faa || sat) {
    if (std::getenv("SIM_NO_TILES")) return -1;
    // .../<z>/<x>/<y>.png
    int z = 0;
    unsigned x = 0, y = 0;
    const std::string u = std::string(url).substr(0, std::string(url).find('?'));
    const size_t p3 = u.rfind('/');
    const size_t p2 = u.rfind('/', p3 - 1);
    const size_t p1 = u.rfind('/', p2 - 1);
    // .../z/x/y.png, or ArcGIS .../tile/z/y/x
    if (faa || sat ? std::sscanf(u.c_str() + p1 + 1, "%d/%u/%u", &z, &y, &x) != 3
            : std::sscanf(u.c_str() + p1 + 1, "%d/%u/%u.png", &z, &x, &y) != 3) {
      return 404;
    }
    body->append(fakeTile(topo || sat, z, x, y, osm, faa));  // (satellite: the topo look will do)
    return 200;
  }
  if (const char* ac = std::strstr(url, "adsbdb.com/v0/aircraft/")) {
    if (!sim::g_route_server) return -1;
    std::string hex(ac + std::strlen("adsbdb.com/v0/aircraft/"));
    hex = hex.substr(0, hex.find('?'));
    for (char& c : hex) c = static_cast<char>(std::tolower(c));
    const std::string json = sim::g_route_server(hex);
    if (json.empty()) return 404;
    body->append(json);
    return 200;
  }
  if (std::strstr(url, "adsbdb.com") != nullptr) return 404;
  if (std::strstr(url, "api.github.com/repos/") != nullptr) {
    // The latest release: the running version, or a newer one with an update image.
    const char* tag = sim::g_update_available ? "v1.13.0" : "v1.12.1";
    *body = std::string(R"({"tag_name":")") + tag + R"(","assets":[{"name":"plane-radar-4in-720x720-chip-v1-)" + tag +
            R"(-ota.bin","size":6000000,"browser_download_url":"https://github.com/sim/ota.bin"}]})";
    return 200;
  }
  const char* name = std::strstr(url, "/trace_full_");
  if (name == nullptr || !sim::g_trace_server) return -1;
  std::string hex(name + std::strlen("/trace_full_"));
  hex = hex.substr(0, hex.find('.'));
  const std::string json = sim::g_trace_server(hex);
  if (json.empty()) return 404;

  z_stream zs{};
  deflateInit2(&zs, Z_BEST_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);  // +16: gzip wrapper
  std::string gz(deflateBound(&zs, json.size()) + 32, '\0');
  zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(json.data()));
  zs.avail_in = static_cast<uInt>(json.size());
  zs.next_out = reinterpret_cast<Bytef*>(&gz[0]);
  zs.avail_out = static_cast<uInt>(gz.size());
  deflate(&zs, Z_FINISH);
  gz.resize(zs.total_out);
  deflateEnd(&zs);
  body->append(gz);
  return 200;
}

void setBrightness(int) {}

int touchPoints(TouchPoint* out, int max) {
  if (!sim::g_touch.down) return 0;
  const int n = sim::g_touch.count < max ? sim::g_touch.count : max;
  for (int i = 0; i < n; ++i) out[i] = {sim::g_touch.x[i], sim::g_touch.y[i]};
  return n;
}

void playSound(core::Sound s) {
  const int id = static_cast<int>(s);
  std::printf("[sound] %s (volume %d%%)\n", soundName(s), s_volume);
  if (s_rendered_sounds.insert(id).second) {
    writeWav(sim::g_out_dir + "/sound_" + soundName(s) + ".wav", core::synthesize(s, 22050), 22050);
  }
}

void setVolume(int percent) { s_volume = percent; }

void startTask(const char*, std::function<void()> fn, size_t, int) { std::thread(std::move(fn)).detach(); }

void sleepMs(uint32_t ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

void restart() { std::printf("[sim] restart requested\n"); }

int httpGetKeepAlive(const char* url, std::string* body, int timeout_ms) {
  return httpGet(url, body, timeout_ms);
}

// SD card: a folder next to the screenshots, "inserted" unless SIM_NO_SD is set.
namespace {
bool s_sd_mounted = false;
std::string s_sd_root;
}  // namespace

const char* sdRoot() {
  s_sd_root = sim::g_out_dir + "/sdcard";
  return s_sd_root.c_str();
}

SdInfo sdInfo() {
  SdInfo info;
  info.mounted = s_sd_mounted;
  if (s_sd_mounted) {
    info.total_bytes = 31914983424ULL;  // a "32 GB" card
    info.free_bytes = 30000000000ULL;
  } else {
    std::snprintf(info.error, sizeof(info.error), "%s", std::getenv("SIM_NO_SD") ? "No card" : "Unmounted");
  }
  return info;
}

bool sdMount(SdInfo* info) {
  if (!s_sd_mounted && !std::getenv("SIM_NO_SD")) {
    const std::string cmd = std::string("mkdir -p '") + sdRoot() + "'";
    s_sd_mounted = std::system(cmd.c_str()) == 0;
  }
  if (info) *info = sdInfo();
  return s_sd_mounted;
}

void sdUnmount() { s_sd_mounted = false; }

// Fixed, repeatable wall clock: 2026-10-27 17:09:25 UTC (10:09:25 Pacific)
// at the start of the run, then advancing with the virtual clock.
int64_t unixTimeMs() { return 1793120965000LL + sim::g_ms; }
bool timeValid() { return true; }

// Real (host) time, for profiling only.
uint64_t micros() {
  return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch())
                                   .count());
}

const char* firmwareVersion() { return "1.12.1"; }
const char* firmwareVariant() { return "4in-720x720-chip-v1"; }
bool installFirmware(const char*, const std::function<void(size_t, size_t)>& progress, std::string* error) {
  if (std::getenv("SIM_OTA_FAIL")) {
    *error = "Download interrupted - try again";
    return false;
  }
  for (size_t done = 0; done <= 6000000; done += 1500000) progress(done, 6000000);
  return true;
}
void confirmFirmware() {}
void* allocLarge(size_t bytes) { return std::aligned_alloc(64, (bytes + 63) / 64 * 64); }
void freeLarge(void* p) { std::free(p); }

size_t freeInternalHeap() { return 412 * 1024; }
size_t freePsram() { return 28 * 1024 * 1024; }
std::string healthReport() { return "RAM (simulator)"; }

}  // namespace plat
