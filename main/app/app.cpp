#include "app/app.h"

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "core/adsb.h"
#include "platform/platform.h"

namespace app {

namespace {

core::Settings s_settings;
core::TrafficStore s_traffic;

bool s_dirty = false;
uint32_t s_dirty_since = 0;
constexpr uint32_t kSaveDebounceMs = 2500;

uint32_t s_last_activity = 0;
bool s_dimmed = false;

std::mutex s_mu;  // guards s_query_* and s_feed
core::LatLon s_query_center;
double s_query_radius_km = 50;
FeedStatus s_feed;
std::atomic<bool> s_refresh_requested{false};

constexpr uint32_t kMinFetchSpacingMs = 1500;  // public APIs allow ~1 req/s

void applyHardware() {
  if (!s_dimmed) plat::setBrightness(s_settings.brightness);
  plat::setVolume(s_settings.volume);
}

void ingestImpl(int code, const std::string& body, std::vector<core::Aircraft>* parsed) {
  core::adsb::ParseOptions opt;
  opt.include_ground = s_settings.show_ground;
  opt.min_alt_ft = s_settings.min_alt_ft > 0 ? s_settings.min_alt_ft : -2000;
  opt.max_alt_ft = s_settings.max_alt_ft >= 60000 ? 1000000 : s_settings.max_alt_ft;
  const bool ok = code == 200 && core::adsb::parse(body.data(), body.size(), opt, parsed);

  {
    std::lock_guard<std::mutex> lock(s_mu);
    s_feed.last_http = code;
    s_feed.last_ok = ok;
    if (ok) {
      s_feed.ever_ok = true;
      s_feed.last_ok_ms = plat::millis();
      s_feed.last_count = static_cast<int>(parsed->size());
      s_feed.error[0] = '\0';
    } else if (code == 200) {
      std::snprintf(s_feed.error, sizeof(s_feed.error), "Bad data from %s",
                    core::adsb::sourceName(s_settings.source));
    } else if (code == 429) {
      std::snprintf(s_feed.error, sizeof(s_feed.error), "Rate limited - slow down polling");
    } else if (code < 0) {
      std::snprintf(s_feed.error, sizeof(s_feed.error), "Network error (%d)", code);
    } else {
      std::snprintf(s_feed.error, sizeof(s_feed.error), "HTTP %d", code);
    }
  }
  if (!ok) return;

  core::AlertZone zone;
  zone.center = {s_settings.home_lat, s_settings.home_lon};
  zone.radius_km = s_settings.alert_radius_km;
  zone.max_alt_ft = s_settings.alert_max_alt_ft;
  const core::UpdateEvents ev = s_traffic.update(*parsed, plat::millis(), zone);

  if (ev.new_emergencies > 0) {
    sound(core::Sound::Emergency);
  } else if (ev.new_close_approaches > 0) {
    sound(core::Sound::Alert);
  } else if (ev.new_aircraft > 0 && ev.new_aircraft <= 3) {
    // Bursts (first fetch after a big pan) would be noise, not information.
    sound(core::Sound::NewAircraft);
  }
}

void networkLoop() {
  std::string body;
  std::vector<core::Aircraft> parsed;
  uint32_t last_fetch = 0;
  bool first = true;

  for (;;) {
    const uint32_t now = plat::millis();
    const uint32_t interval = s_settings.poll_interval_s * 1000u;
    const bool due = first || now - last_fetch >= interval ||
                     (s_refresh_requested.load() && now - last_fetch >= kMinFetchSpacingMs);
    if (!due || plat::wifiStatus().state != plat::WifiState::Connected) {
      plat::sleepMs(100);
      continue;
    }
    first = false;
    s_refresh_requested = false;
    last_fetch = now;

    core::LatLon center;
    double radius_km;
    {
      std::lock_guard<std::mutex> lock(s_mu);
      center = s_query_center;
      radius_km = s_query_radius_km;
    }
    // Fetch a bit past the screen edge so aircraft slide in instead of popping.
    const std::string url =
        core::adsb::buildUrl(s_settings.source, center.lat, center.lon, radius_km * 1.25 + 5.0);

    body.clear();
    const int code = plat::httpGet(url.c_str(), &body, 10000);
    ingestImpl(code, body, &parsed);
  }
}

}  // namespace

void init() {
  plat::settingsLoad(&s_settings);
  s_query_center = {s_settings.home_lat, s_settings.home_lon};
  s_query_radius_km = s_settings.home_radius_km;
  s_last_activity = plat::millis();
  applyHardware();
  if (s_settings.wifi_ssid[0] != '\0') {
    plat::wifiConnect(s_settings.wifi_ssid, s_settings.wifi_pass);
  }
}

core::Settings& settings() { return s_settings; }

void settingsChanged() {
  applyHardware();
  if (!s_dirty) s_dirty_since = plat::millis();
  s_dirty = true;
}

void settingsFlush() {
  if (!s_dirty) return;
  plat::settingsSave(s_settings);
  s_dirty = false;
}

void tick(uint32_t now_ms) {
  if (s_dirty && now_ms - s_dirty_since >= kSaveDebounceMs) settingsFlush();

  if (s_settings.dim_after_min > 0 && !s_dimmed &&
      now_ms - s_last_activity >= s_settings.dim_after_min * 60000u) {
    s_dimmed = true;
    plat::setBrightness(s_settings.dim_brightness);
  }
}

bool userActivity() {
  s_last_activity = plat::millis();
  if (!s_dimmed) return false;
  s_dimmed = false;
  plat::setBrightness(s_settings.brightness);
  return true;
}

core::TrafficStore& traffic() { return s_traffic; }

void setQueryArea(const core::LatLon& center, double radius_km) {
  std::lock_guard<std::mutex> lock(s_mu);
  s_query_center = center;
  s_query_radius_km = radius_km;
}

void queryArea(core::LatLon* center, double* radius_km) {
  std::lock_guard<std::mutex> lock(s_mu);
  *center = s_query_center;
  *radius_km = s_query_radius_km;
}

void requestRefresh() { s_refresh_requested = true; }

FeedStatus feedStatus() {
  std::lock_guard<std::mutex> lock(s_mu);
  return s_feed;
}

void ingest(int http_code, const std::string& body) {
  std::vector<core::Aircraft> parsed;
  ingestImpl(http_code, body, &parsed);
}

void startNetworkTask() { plat::startTask("adsb", networkLoop, 16 * 1024, 4); }

void sound(core::Sound s) {
  const core::Settings& st = s_settings;
  if (!st.sound_enabled) return;
  switch (s) {
    case core::Sound::Click: if (!st.sound_touch) return; break;
    case core::Sound::Ping: if (!st.sound_sweep_ping) return; break;
    case core::Sound::NewAircraft: if (!st.sound_new_aircraft) return; break;
    case core::Sound::Alert:
    case core::Sound::Emergency: if (!st.sound_alerts) return; break;
    default: break;
  }
  plat::playSound(s);
}

double kmToUnits(double km) {
  switch (s_settings.units) {
    case core::Units::Mi: return km / core::kKmPerMi;
    case core::Units::Nm: return km / core::kKmPerNm;
    default: return km;
  }
}

double unitsToKm(double v) {
  switch (s_settings.units) {
    case core::Units::Mi: return v * core::kKmPerMi;
    case core::Units::Nm: return v * core::kKmPerNm;
    default: return v;
  }
}

const char* unitSuffix() {
  switch (s_settings.units) {
    case core::Units::Mi: return "mi";
    case core::Units::Nm: return "nm";
    default: return "km";
  }
}

void formatDistance(double km, char* out, int out_len, bool compact) {
  const double v = kmToUnits(km);
  const char* sep = compact ? "" : " ";
  if (v < 10.0 && std::fabs(v - std::round(v)) > 0.05) {
    std::snprintf(out, out_len, "%.1f%s%s", v, sep, unitSuffix());
  } else {
    std::snprintf(out, out_len, "%.0f%s%s", v, sep, unitSuffix());
  }
}

}  // namespace app
