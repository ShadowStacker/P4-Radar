#include "app/app.h"

#include <algorithm>
#include <atomic>
#include <deque>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "core/adsb.h"
#include "core/filter.h"
#include "app/maps.h"
#include "app/wifi_internal.h"
#include "core/inflate.h"
#include "core/route.h"
#include "core/settings_text.h"
#include "core/trace.h"
#include "platform/platform.h"

namespace app {

namespace {

core::Settings s_settings;
core::TrafficStore s_traffic;

bool s_dirty = false;
uint32_t s_generation = 0;
uint32_t s_dirty_since = 0;
constexpr uint32_t kSaveDebounceMs = 2500;

uint32_t s_last_activity = 0;
bool s_dimmed = false;
bool s_sleeping = false;  // screen off and sounds muted until a tap (or an alarm)

// While the screen is being used, the poller holds back heavy background work
// (tile decoding, flight histories). It runs on the other core, but the two
// cores share the memory cache, so streaming megabytes through PSRAM slows
// the UI down too.
std::atomic<uint32_t> s_last_touch_ms{0};
std::atomic<bool> s_menu_open{false};
constexpr uint32_t kTouchQuietMs = 1500;  // background work waits this long after a touch
constexpr uint32_t kMaxFeedDelayMs = 3000;  // but traffic updates are never later than this

// Alarms (UI task only).
int s_ringing = -1;
uint32_t s_ring_started_ms = 0;
uint32_t s_next_beep_ms = 0;
int64_t s_last_minute = -1;
int s_snoozed = -1;
int64_t s_snooze_until_s = 0;
constexpr uint32_t kBeepEveryMs = 2000;

std::mutex s_mu;  // guards s_query_* and s_feed
core::LatLon s_query_center;
double s_query_radius_km = 50;
FeedStatus s_feed;
std::atomic<bool> s_refresh_requested{false};
std::deque<std::string> s_notices;  // guarded by s_mu

std::mutex s_favorites_mu;
std::vector<core::Favorite> s_favorites;

constexpr uint32_t kMinFetchSpacingMs = 1500;  // public APIs allow ~1 req/s

// Extended trail. Guarded by s_trace_mu.
std::mutex s_trace_mu;
TraceInfo s_trace_info;
core::trace::Trace s_trace;
bool s_trace_pending = false;
uint32_t s_trace_fetched_ms = 0;
constexpr uint32_t kTraceRefreshMs = 120000;      // keep the history joined to the live trail
constexpr uint32_t kTraceRetryMs = 300000;        // after "not available"
constexpr size_t kMaxTraceJson = 8 * 1024 * 1024;  // decompressed

// Airline / route lookups. Guarded by s_route_mu.
std::mutex s_route_mu;
RouteState s_route;
bool s_route_pending = false;
std::deque<RouteState> s_route_cache;  // most recent first
// Favorites waiting to be looked up: tail number or hex, and whether it came
// from favorites.txt.
std::deque<std::pair<std::string, bool>> s_fav_lookups;
constexpr size_t kRouteCacheSize = 24;

// Text copies of the settings and favorites on the SD card. Guarded by s_files_mu.
std::mutex s_files_mu;
bool s_first_mount_done = false;
bool s_settings_file_dirty = false;   // settings.txt is behind; write s_settings_file_text
std::string s_settings_file_text;
bool s_favorites_file_dirty = false;  // favorites.txt is behind
bool s_settings_file_read = false;    // s_settings_file_in waits for the UI task
std::string s_settings_file_in;
bool s_settings_file_missing = false;  // UI task should write one
std::vector<std::string> s_fav_unresolved;  // tail numbers from favorites.txt not looked up yet
std::function<void(const core::Settings&)> s_on_loaded;  // UI task
constexpr size_t kMaxTextFile = 64 * 1024;

std::string sdPath(const char* name) { return std::string(plat::sdRoot()) + "/" + name; }

bool readText(const char* name, std::string* out) {
  FILE* f = std::fopen(sdPath(name).c_str(), "rb");
  if (f == nullptr) return false;
  out->clear();
  char buf[1024];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0 && out->size() < kMaxTextFile) out->append(buf, n);
  std::fclose(f);
  return true;
}

void writeText(const char* name, const std::string& text) {
  // Write a temporary file and rename it, so a power cut never leaves half a file.
  const std::string path = sdPath(name), tmp = path + ".tmp";
  FILE* f = std::fopen(tmp.c_str(), "wb");
  bool ok = f != nullptr && std::fwrite(text.data(), 1, text.size(), f) == text.size();
  if (f != nullptr && std::fclose(f) != 0) ok = false;
  if (ok) {
    std::remove(path.c_str());
    ok = std::rename(tmp.c_str(), path.c_str()) == 0;
  } else {
    std::remove(tmp.c_str());
  }
  std::printf("sd: %s %s (%u bytes)\n", ok ? "wrote" : "FAILED to write", name, static_cast<unsigned>(text.size()));
}

/** UI task: settings.txt should match s_settings. */
void queueSettingsFile() {
  std::string text = core::settings_text::serialize(s_settings);
  std::lock_guard<std::mutex> lock(s_files_mu);
  s_settings_file_text.swap(text);
  s_settings_file_dirty = true;
}

void queueFavoritesFile() {
  std::lock_guard<std::mutex> lock(s_files_mu);
  s_favorites_file_dirty = true;
}

void dropUnresolved(const std::string& tail) {
  std::lock_guard<std::mutex> lock(s_files_mu);
  s_fav_unresolved.erase(std::remove(s_fav_unresolved.begin(), s_fav_unresolved.end(), tail),
                         s_fav_unresolved.end());
  s_favorites_file_dirty = true;
}

void settingsFlushImpl();

/** UI task: applies a settings.txt read by the network task. */
void applySettingsFile() {
  std::string text;
  bool read, missing;
  {
    std::lock_guard<std::mutex> lock(s_files_mu);
    read = s_settings_file_read;
    missing = s_settings_file_missing;
    s_settings_file_read = s_settings_file_missing = false;
    text.swap(s_settings_file_in);
  }
  if (missing) queueSettingsFile();
  if (!read) return;

  core::Settings next = s_settings;
  std::vector<std::string> problems;
  const int n = core::settings_text::parse(text, &next, &problems);
  std::printf("sd: settings.txt has %d settings\n", n);
  for (const std::string& p : problems) std::printf("sd: settings.txt %s\n", p.c_str());

  if (std::memcmp(&next, &s_settings, sizeof(next)) == 0) {
    // Same settings; rewrite the file if it's missing lines or in an old layout.
    if (core::settings_text::serialize(s_settings) != text) queueSettingsFile();
    return;
  }
  const core::Settings before = s_settings;
  s_settings = next;
  core::applyTimeZone(s_settings.time_zone);
  if (std::strcmp(before.wifi_ssid, s_settings.wifi_ssid) != 0 ||
      std::strcmp(before.wifi_pass, s_settings.wifi_pass) != 0) {
    plat::wifiConnect(s_settings.wifi_ssid, s_settings.wifi_pass);
  }
  settingsChanged();
  settingsFlushImpl();  // saves to flash and rewrites the file in the current layout
  traffic().clear();     // filters may have changed
  requestRefresh();
  {
    std::lock_guard<std::mutex> lock(s_mu);
    s_notices.push_back(problems.empty() ? "Settings loaded from SD card"
                                         : "Settings loaded from SD card (some lines ignored)");
  }
  if (s_on_loaded) s_on_loaded(before);
}

bool sameKey(const RouteState& r, const char* hex, const char* callsign) {
  return std::strcmp(r.hex, hex) == 0 && std::strcmp(r.callsign, callsign) == 0;
}

/** One adsbdb GET merged into `info`; returns the HTTP status. */
int routeGet(const std::string& url, core::route::Info* info) {
  std::string body;
  const int code = plat::httpGet(url.c_str(), &body, 10000);
  if (code == 200 && !core::route::parse(body.data(), body.size(), info)) return -2;
  std::printf("route: %s -> %d\n", url.c_str(), code);
  return code;
}

/** Tries each trace host in turn; true once one returned a usable trace. */
bool downloadTrace(const char* hex, core::trace::Trace* out) {
  std::string body, json;
  for (const std::string& host : core::trace::hostsFor(s_settings.source)) {
    const std::string url = core::trace::buildUrl(host, hex);
    const std::string referer = "https://" + host + "/";
    const char* const headers[] = {"Accept-Encoding", "gzip", "Referer", referer.c_str(), nullptr};
    body.clear();
    const int code = plat::httpGet(url.c_str(), &body, 15000, headers);
    if (code != 200 || body.empty()) {
      std::printf("trace: %s -> HTTP %d\n", url.c_str(), code);
      continue;
    }
    const std::string* src = &body;
    if (core::isGzip(body.data(), body.size())) {
      json.reserve(body.size() * 6);
      if (!core::gunzip(body.data(), body.size(), &json, kMaxTraceJson)) {
        std::printf("trace: %s -> bad gzip data\n", url.c_str());
        continue;
      }
      src = &json;
    }
    if (core::trace::parse(src->data(), src->size(), out)) {
      std::printf("trace: %s -> %u points, %.0f min\n", url.c_str(),
                  static_cast<unsigned>(out->points.size()), out->durationS() / 60.0f);
      return true;
    }
    std::printf("trace: %s -> no usable trace\n", url.c_str());
  }
  return false;
}

void applyHardware() {
  if (s_sleeping) plat::setBrightness(0);
  else if (!s_dimmed) plat::setBrightness(s_settings.brightness);
  // A ringing alarm is never quieter than 70 %, whatever the volume setting.
  plat::setVolume(s_ringing >= 0 ? std::max<int>(s_settings.volume, 70) : s_settings.volume);
}

void startRinging(int index) {
  s_ringing = index;
  s_ring_started_ms = plat::millis();
  s_next_beep_ms = s_ring_started_ms;
  s_snoozed = -1;
  s_snooze_until_s = 0;
  s_sleeping = false;  // only reached when asleep with "Wake on alarm" on
  userActivity();      // wakes a dimmed screen too
  applyHardware();
}

void checkAlarms(uint32_t now_ms) {
  if (plat::timeValid()) {
    const int64_t unix_s = plat::unixTimeMs() / 1000;
    const int64_t minute = unix_s / 60;
    if (minute != s_last_minute) {
      s_last_minute = minute;
      const std::tm t = core::localTime(unix_s);
      for (int i = 0; i < core::kMaxAlarms; ++i) {
        core::Alarm& a = s_settings.alarms[i];
        if (!core::alarmMatches(a, t)) continue;
        if (a.days == 0) {  // one-time alarm: done after this
          a.enabled = false;
          settingsChanged();
        }
        // Asleep without "Wake on alarm": the alarm passes silently; only a
        // tap wakes the screen.
        if (s_ringing < 0 && (!s_sleeping || s_settings.wake_on_alarm)) startRinging(i);
      }
    }
    if (s_snoozed >= 0 && unix_s >= s_snooze_until_s && s_ringing < 0) {
      if (!s_sleeping || s_settings.wake_on_alarm) {
        startRinging(s_snoozed);
      } else {
        s_snoozed = -1;  // snoozed, then put to sleep: drop it
        s_snooze_until_s = 0;
      }
    }
  }
  if (s_ringing >= 0) {
    if (now_ms - s_ring_started_ms > core::kAlarmRingMinutes * 60000u) {
      stopAlarm();
    } else if (static_cast<int32_t>(now_ms - s_next_beep_ms) >= 0) {
      plat::playSound(core::Sound::Alarm);
      s_next_beep_ms = now_ms + kBeepEveryMs;
    }
  }
}

void ingestImpl(int code, const std::string& body, std::vector<core::Aircraft>* parsed) {
  // Parse everything, then apply the display filters here so favorites can
  // bypass them (a saved aircraft is never hidden, and still raises alerts).
  core::adsb::ParseOptions opt;
  opt.include_ground = true;
  opt.min_alt_ft = -100000;
  opt.max_alt_ft = 1000000;
  const bool ok = code == 200 && core::adsb::parse(body.data(), body.size(), opt, parsed);
  const int total = ok ? static_cast<int>(parsed->size()) : 0;
  if (ok) {
    const std::vector<core::Favorite> favs = favorites();
    parsed->erase(std::remove_if(parsed->begin(), parsed->end(),
                                 [&](const core::Aircraft& a) {
                                   return !core::passesFilters(a, s_settings) && !core::containsHex(favs, a.hex);
                                 }),
                  parsed->end());
  }

  {
    std::lock_guard<std::mutex> lock(s_mu);
    s_feed.last_http = code;
    s_feed.last_ok = ok;
    if (ok) {
      s_feed.ever_ok = true;
      s_feed.last_ok_ms = plat::millis();
      s_feed.last_count = static_cast<int>(parsed->size());
      s_feed.last_total = total;
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
  const std::vector<core::Favorite> favs = favorites();
  core::FavoriteWatch watch;
  watch.favorites = &favs;
  watch.center = zone.center;
  watch.near_radius_km = s_settings.favorite_near_radius_km;
  const core::UpdateEvents ev = s_traffic.update(*parsed, plat::millis(), zone, watch);

  // On-screen notices for favorites (shown by the UI task), whatever the sound settings.
  {
    std::lock_guard<std::mutex> lock(s_mu);
    for (const std::string& n : ev.favorites_near) s_notices.push_back(n + " is nearby");
    for (const std::string& n : ev.favorites_seen) {
      if (std::find(ev.favorites_near.begin(), ev.favorites_near.end(), n) == ev.favorites_near.end()) {
        s_notices.push_back(n + " detected");
      }
    }
    while (s_notices.size() > 4) s_notices.pop_front();
  }

  // One sound per fetch, most important first.
  if (ev.new_emergencies > 0) {
    sound(core::Sound::Emergency);
  } else if (!ev.favorites_near.empty() && s_settings.sound_favorite_near) {
    sound(core::Sound::FavoriteNear);
  } else if (!ev.favorites_seen.empty() && s_settings.sound_favorite_seen) {
    sound(core::Sound::FavoriteSeen);
  } else if (ev.new_close_approaches > 0 && s_settings.sound_proximity) {
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
    // Someone is touching the screen: wait, unless traffic would be late.
    const bool touching = now - s_last_touch_ms.load() < kTouchQuietMs;
    if (touching && !(first || now - last_fetch >= interval + kMaxFeedDelayMs)) {
      plat::sleepMs(50);
      continue;
    }
    const bool menu = s_menu_open.load();  // radar hidden: no tiles or flight history needed
    // The SD card and saved map tiles work without Wi-Fi.
    if (maps::serviceSd() || serviceSdFiles()) continue;
    if (plat::wifiStatus().state != plat::WifiState::Connected) {
      if (menu || !maps::service(false)) plat::sleepMs(100);
      continue;
    }
    if (serviceUpdate()) continue;  // asked for on the About page
    if (!first && now - last_fetch >= kMinFetchSpacingMs &&
        (serviceFavoriteLookup() || (!menu && (serviceRoute() || serviceTrace())))) {
      last_fetch = plat::millis();
      continue;
    }
    if (!due) {
      // Map tiles fill the gaps between traffic updates (different servers,
      // so the ADS-B rate limit doesn't apply).
      if (menu || !maps::service(true)) plat::sleepMs(100);
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
  plat::favoritesLoad(&s_favorites);
  s_query_center = {s_settings.home_lat, s_settings.home_lon};
  s_query_radius_km = s_settings.home_radius_km;
  s_last_activity = plat::millis();
  core::applyTimeZone(s_settings.time_zone);
  applyHardware();
  if (s_settings.wifi_ssid[0] != '\0') {
    plat::wifiConnect(s_settings.wifi_ssid, s_settings.wifi_pass);
  }
}

core::Settings& settings() { return s_settings; }

void settingsChanged() {
  ++s_generation;
  applyHardware();
  if (!s_dirty) s_dirty_since = plat::millis();
  s_dirty = true;
}

uint32_t settingsGeneration() { return s_generation; }

void settingsFlush() { settingsFlushImpl(); }

namespace {
void settingsFlushImpl() {
  if (!s_dirty) return;
  plat::settingsSave(s_settings);
  queueSettingsFile();
  s_dirty = false;
}
}  // namespace

void onSettingsLoaded(std::function<void(const core::Settings& before)> fn) { s_on_loaded = std::move(fn); }

void factoryReset() {
  // favorites.txt is kept: it's the user's own list and restores the favorites.
  if (maps::sdStatus().state == maps::SdState::Mounted) {
    std::remove(sdPath(core::settings_text::kSettingsFile).c_str());
  }
  plat::settingsErase();
}

void sdFilesMounted() {
  bool first, settings_behind, favorites_behind;
  {
    std::lock_guard<std::mutex> lock(s_files_mu);
    first = !s_first_mount_done;
    s_first_mount_done = true;
    settings_behind = s_settings_file_dirty;
    favorites_behind = s_favorites_file_dirty;
  }
  // At boot the card wins. A card put in later only wins if nothing changed
  // on the screen in the meantime; otherwise it gets this board's settings.
  std::string text;
  if (first || !settings_behind) {
    const bool found = readText(core::settings_text::kSettingsFile, &text);
    std::lock_guard<std::mutex> lock(s_files_mu);
    s_settings_file_dirty = false;
    if (found) {
      s_settings_file_in.swap(text);
      s_settings_file_read = true;
    } else {
      s_settings_file_missing = true;
    }
  }
  if (first || !favorites_behind) {
    if (readText(core::settings_text::kFavoritesFile, &text)) {
      std::vector<core::Favorite> favs;
      std::vector<std::string> lookups;
      core::settings_text::parseFavorites(text, &favs, &lookups);
      std::printf("sd: favorites.txt has %u aircraft, %u to look up\n", static_cast<unsigned>(favs.size()),
                  static_cast<unsigned>(lookups.size()));
      {
        std::lock_guard<std::mutex> lock(s_favorites_mu);
        s_favorites = favs;
      }
      plat::favoritesSave(favs);
      {
        std::lock_guard<std::mutex> lock(s_files_mu);
        s_fav_unresolved = lookups;
        s_favorites_file_dirty = false;
      }
      std::lock_guard<std::mutex> lock(s_route_mu);
      for (const std::string& q : lookups) s_fav_lookups.emplace_back(q, true);
    } else {
      queueFavoritesFile();
    }
  }
}

bool serviceSdFiles() {
  if (maps::sdStatus().state != maps::SdState::Mounted) return false;
  std::string text;
  {
    std::lock_guard<std::mutex> lock(s_files_mu);
    if (s_settings_file_dirty) {
      text.swap(s_settings_file_text);
      s_settings_file_dirty = false;
    }
  }
  if (!text.empty()) {
    writeText(core::settings_text::kSettingsFile, text);
    return true;
  }
  std::vector<std::string> unresolved;
  {
    std::lock_guard<std::mutex> lock(s_files_mu);
    if (!s_favorites_file_dirty) return false;
    s_favorites_file_dirty = false;
    unresolved = s_fav_unresolved;
  }
  writeText(core::settings_text::kFavoritesFile, core::settings_text::serializeFavorites(favorites(), unresolved));
  return true;
}

void tick(uint32_t now_ms) {
  // A fresh update has run a minute without crashing: keep it (otherwise the
  // next reset goes back to the previous version).
  static bool s_confirmed = false;
  if (!s_confirmed && now_ms > 60000) {
    s_confirmed = true;
    plat::confirmFirmware();
  }
  applySettingsFile();
  serviceWifi(now_ms);
  checkAlarms(now_ms);
  if (s_dirty && now_ms - s_dirty_since >= kSaveDebounceMs) settingsFlush();

  if (s_settings.dim_after_min > 0 && !s_dimmed && !s_sleeping &&
      now_ms - s_last_activity >= s_settings.dim_after_min * 60000u) {
    s_dimmed = true;
    plat::setBrightness(s_settings.dim_brightness);
  }
}

void timeZoneChanged() {
  core::applyTimeZone(s_settings.time_zone);
  s_last_minute = -1;
}

int alarmRinging() { return s_ringing; }

void snoozeAlarm() {
  if (s_ringing < 0) return;
  s_snoozed = s_ringing;
  s_snooze_until_s = plat::unixTimeMs() / 1000 + core::kSnoozeMinutes * 60;
  s_ringing = -1;
  applyHardware();
}

void stopAlarm() {
  s_ringing = -1;
  s_snoozed = -1;
  s_snooze_until_s = 0;
  applyHardware();
}

void testAlarm(int index) { startRinging(index); }

int snoozeMinutesLeft() {
  if (s_snoozed < 0) return -1;
  const int64_t left = s_snooze_until_s - plat::unixTimeMs() / 1000;
  return left <= 0 ? 0 : static_cast<int>((left + 59) / 60);
}

void setSleeping(bool on) {
  if (on == s_sleeping) return;
  s_sleeping = on;
  s_dimmed = false;
  s_last_activity = plat::millis();
  applyHardware();
}

bool sleeping() { return s_sleeping; }

void setMenuOpen(bool open) { s_menu_open = open; }

bool userActivity() {
  s_last_activity = plat::millis();
  s_last_touch_ms = s_last_activity;
  if (s_sleeping || !s_dimmed) return false;  // waking from sleep is the UI's job (one tap)
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

std::vector<core::Favorite> favorites() {
  std::lock_guard<std::mutex> lock(s_favorites_mu);
  return s_favorites;
}

bool isFavorite(const char* hex) {
  std::lock_guard<std::mutex> lock(s_favorites_mu);
  return core::containsHex(s_favorites, hex);
}

bool toggleFavorite(const core::Aircraft& a) {
  bool now_favorite;
  std::vector<core::Favorite> copy;
  {
    std::lock_guard<std::mutex> lock(s_favorites_mu);
    auto it = std::find_if(s_favorites.begin(), s_favorites.end(),
                           [&](const core::Favorite& f) { return std::strcmp(f.hex, a.hex) == 0; });
    if (it != s_favorites.end()) {
      s_favorites.erase(it);
      now_favorite = false;
    } else {
      if (s_favorites.size() >= core::kMaxFavorites) return false;
      core::Favorite f;
      std::snprintf(f.hex, sizeof(f.hex), "%s", a.hex);
      std::snprintf(f.label, sizeof(f.label), "%s", a.callsign[0] ? a.callsign : (a.reg[0] ? a.reg : a.hex));
      std::snprintf(f.type, sizeof(f.type), "%s", a.type);
      s_favorites.push_back(f);
      now_favorite = true;
    }
    copy = s_favorites;
  }
  plat::favoritesSave(copy);
  queueFavoritesFile();
  return now_favorite;
}

void removeFavorite(const char* hex) {
  std::vector<core::Favorite> copy;
  {
    std::lock_guard<std::mutex> lock(s_favorites_mu);
    s_favorites.erase(std::remove_if(s_favorites.begin(), s_favorites.end(),
                                     [&](const core::Favorite& f) { return std::strcmp(f.hex, hex) == 0; }),
                      s_favorites.end());
    copy = s_favorites;
  }
  plat::favoritesSave(copy);
  queueFavoritesFile();
}

bool addFavorite(const core::Favorite& f) {
  std::vector<core::Favorite> copy;
  {
    std::lock_guard<std::mutex> lock(s_favorites_mu);
    if (core::containsHex(s_favorites, f.hex) || s_favorites.size() >= core::kMaxFavorites) return false;
    s_favorites.push_back(f);
    copy = s_favorites;
  }
  plat::favoritesSave(copy);
  queueFavoritesFile();
  return true;
}

void lookupFavorite(const std::string& q) {
  std::lock_guard<std::mutex> lock(s_route_mu);
  for (const auto& p : s_fav_lookups) {
    if (p.first == q) return;
  }
  s_fav_lookups.emplace_back(q, false);
}

bool serviceFavoriteLookup() {
  std::string q;
  bool from_file;
  {
    std::lock_guard<std::mutex> lock(s_route_mu);
    if (s_fav_lookups.empty()) return false;
    q = s_fav_lookups.front().first;
    from_file = s_fav_lookups.front().second;
    s_fav_lookups.pop_front();
  }
  core::route::Info info;
  std::string body;
  const std::string url = core::route::lookupUrl(q.c_str());
  const int code = plat::httpGet(url.c_str(), &body, 10000);
  const bool found = code == 200 && core::route::parse(body.data(), body.size(), &info) && info.mode_s[0] != '\0';
  std::printf("favorite lookup: %s -> %d\n", url.c_str(), code);

  // A tail number from favorites.txt leaves the file once it's resolved (or
  // doesn't exist); after a network error it stays for the next start.
  if (from_file && (found || code == 404 || code == 200)) dropUnresolved(q);
  std::string notice;
  if (found) {
    core::Favorite f;
    std::snprintf(f.hex, sizeof(f.hex), "%s", info.mode_s);
    std::snprintf(f.label, sizeof(f.label), "%s", info.registration[0] ? info.registration : q.c_str());
    std::snprintf(f.type, sizeof(f.type), "%s", info.icao_type);
    if (isFavorite(f.hex)) notice = std::string(f.label) + " is already a favorite";
    else if (!addFavorite(f)) notice = "Favorites full (32) - remove one first";
    else notice = std::string(f.label) + " added to favorites";
  } else if (code == 404 || code == 200) {
    notice = q + " not found - check the tail number";
  } else {
    notice = "Couldn't look up " + q + " (no internet?)";
  }
  std::lock_guard<std::mutex> lock(s_mu);
  s_notices.push_back(notice);
  return true;
}

void setFavorites(const std::vector<core::Favorite>& favs) {
  {
    std::lock_guard<std::mutex> lock(s_favorites_mu);
    s_favorites = favs;
  }
  plat::favoritesSave(favs);
  queueFavoritesFile();
}

bool popNotice(std::string* out) {
  std::lock_guard<std::mutex> lock(s_mu);
  if (s_notices.empty()) return false;
  *out = s_notices.front();
  s_notices.pop_front();
  return true;
}

FeedStatus feedStatus() {
  std::lock_guard<std::mutex> lock(s_mu);
  return s_feed;
}

void ingest(int http_code, const std::string& body) {
  std::vector<core::Aircraft> parsed;
  ingestImpl(http_code, body, &parsed);
}

void requestTrace(const char* hex) {
  std::lock_guard<std::mutex> lock(s_trace_mu);
  if (!s_settings.extended_trail) hex = "";
  if (std::strcmp(hex, s_trace_info.hex) == 0) return;
  std::snprintf(s_trace_info.hex, sizeof(s_trace_info.hex), "%s", hex);
  s_trace.points.clear();
  s_trace.points.shrink_to_fit();
  s_trace_info.status = hex[0] ? TraceStatus::Loading : TraceStatus::None;
  s_trace_info.duration_s = 0;
  s_trace_info.version++;
  s_trace_pending = hex[0] != '\0';
}

TraceInfo traceInfo() {
  std::lock_guard<std::mutex> lock(s_trace_mu);
  return s_trace_info;
}

uint32_t traceCopy(core::trace::Trace* out) {
  std::lock_guard<std::mutex> lock(s_trace_mu);
  *out = s_trace;
  return s_trace_info.version;
}

void requestRoute(const char* hex, const char* callsign) {
  std::lock_guard<std::mutex> lock(s_route_mu);
  if (!s_settings.show_route_info) hex = "";
  if (hex[0] == '\0') callsign = "";
  if (sameKey(s_route, hex, callsign)) return;
  s_route = RouteState{};
  std::snprintf(s_route.hex, sizeof(s_route.hex), "%s", hex);
  std::snprintf(s_route.callsign, sizeof(s_route.callsign), "%s", callsign);
  s_route_pending = false;
  if (hex[0] == '\0') return;
  for (const RouteState& c : s_route_cache) {
    if (sameKey(c, hex, callsign)) {
      s_route = c;
      return;
    }
  }
  s_route.status = RouteStatus::Loading;
  s_route_pending = true;
}

RouteState routeState() {
  std::lock_guard<std::mutex> lock(s_route_mu);
  return s_route;
}

bool serviceRoute() {
  char hex[8], callsign[9];
  {
    std::lock_guard<std::mutex> lock(s_route_mu);
    if (!s_route_pending) return false;
    s_route_pending = false;
    std::memcpy(hex, s_route.hex, sizeof(hex));
    std::memcpy(callsign, s_route.callsign, sizeof(callsign));
  }

  core::route::Info info;
  int code = routeGet(core::route::aircraftUrl(hex, callsign), &info);
  bool reached = code == 200 || code == 404;
  // Unknown airframe (404): the callsign alone may still have a route.
  if (code == 404 && callsign[0]) reached = routeGet(core::route::callsignUrl(callsign), &info) > 0;
  // No route: at least name the airline from the callsign prefix.
  const std::string prefix = core::route::airlinePrefix(callsign);
  if (reached && info.airline[0] == '\0' && !prefix.empty()) routeGet(core::route::airlineUrl(prefix.c_str()), &info);

  std::lock_guard<std::mutex> lock(s_route_mu);
  RouteState done;
  std::memcpy(done.hex, hex, sizeof(hex));
  std::memcpy(done.callsign, callsign, sizeof(callsign));
  done.info = info;
  const bool any = info.airline[0] || info.model[0] || info.owner[0] || info.has_route;
  done.status = any ? RouteStatus::Ready : RouteStatus::Unknown;
  // Network errors aren't cached, so the next tap tries again.
  if (reached) {
    s_route_cache.push_front(done);
    if (s_route_cache.size() > kRouteCacheSize) s_route_cache.pop_back();
  }
  if (sameKey(s_route, hex, callsign)) s_route = done;
  return true;
}

bool serviceTrace() {
  char hex[8];
  {
    std::lock_guard<std::mutex> lock(s_trace_mu);
    const uint32_t age = plat::millis() - s_trace_fetched_ms;
    if (s_trace_info.hex[0] != '\0' && !s_trace_pending) {
      if ((s_trace_info.status == TraceStatus::Ready && age >= kTraceRefreshMs) ||
          (s_trace_info.status == TraceStatus::Unavailable && age >= kTraceRetryMs)) {
        s_trace_pending = true;
      }
    }
    if (!s_trace_pending) return false;
    s_trace_pending = false;
    std::memcpy(hex, s_trace_info.hex, sizeof(hex));
  }

  core::trace::Trace fresh;
  const bool ok = downloadTrace(hex, &fresh);

  std::lock_guard<std::mutex> lock(s_trace_mu);
  s_trace_fetched_ms = plat::millis();
  if (std::strcmp(hex, s_trace_info.hex) != 0) return true;  // selection changed meanwhile
  if (ok) {
    s_trace = std::move(fresh);
    s_trace_info.status = TraceStatus::Ready;
    s_trace_info.duration_s = s_trace.durationS();
    s_trace_info.version++;
  } else if (s_trace_info.status != TraceStatus::Ready) {
    // Keep showing an older copy if a refresh fails.
    s_trace_info.status = TraceStatus::Unavailable;
    s_trace_info.version++;
  }
  return true;
}

void startNetworkTask() { plat::startTask("adsb", networkLoop, 16 * 1024, 4); }

void sound(core::Sound s) {
  const core::Settings& st = s_settings;
  if (!st.sound_enabled || s_sleeping) return;
  switch (s) {
    case core::Sound::Click: if (!st.sound_touch) return; break;
    case core::Sound::Ping: if (!st.sound_sweep_ping) return; break;
    case core::Sound::NewAircraft: if (!st.sound_new_aircraft) return; break;
    case core::Sound::Alert: if (!st.sound_proximity) return; break;
    case core::Sound::Emergency: if (!st.sound_alerts) return; break;
    case core::Sound::FavoriteSeen: if (!st.sound_favorite_seen) return; break;
    case core::Sound::FavoriteNear: if (!st.sound_favorite_near) return; break;
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
