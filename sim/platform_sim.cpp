// Fake implementations of platform.h for the desktop simulator.

#include <chrono>
#include <cstdio>
#include <cstring>
#include <set>
#include <thread>

#include "app/app.h"
#include "platform/platform.h"
#include "sim_state.h"

namespace sim {
uint32_t g_ms = 1000;
Touch g_touch;
bool g_preset_wifi = true;
std::string g_out_dir = "sim_out";
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

int httpGet(const char*, std::string*, int) { return -1; }

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

const char* firmwareVersion() { return "sim"; }
size_t freeInternalHeap() { return 412 * 1024; }
size_t freePsram() { return 28 * 1024 * 1024; }

}  // namespace plat
