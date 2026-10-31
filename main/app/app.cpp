#include "app/app.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <deque>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "core/adsb.h"
#include "core/failover.h"
#include "core/filter.h"
#include "app/maps.h"
#include "app/web.h"
#include "app/wifi_internal.h"
#include "core/inflate.h"
#include "core/lookup_cache.h"
#include "core/png.h"
#include "core/sun.h"
#include "core/watch.h"
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
// Saving writes flash, which pauses both cores (the UI too) for a few ms, or
// up to a few hundred when a flash sector has to be erased. In the settings
// menu, where one change follows another, save on leaving it instead (and at
// least once a minute).
constexpr uint32_t kSaveInMenuMs = 60000;

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
core::SourceFailover s_failover;    // network task only

std::mutex s_favorites_mu;  // also guards s_silenced
std::vector<core::Favorite> s_favorites;
std::vector<core::Favorite> s_silenced;  // no sounds for these (but emergencies)

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
void applySettings(const core::Settings& next);

// Daily log. s_day is written by the network task; guarded by s_day_mu.
std::mutex s_day_mu;
core::daylog::Day s_day;
bool s_day_dirty = false;
bool s_day_loaded = false;      // tried to continue today's file from the card
uint32_t s_day_saved_ms = 0;
constexpr uint32_t kDaySaveEveryMs = 10 * 60 * 1000;

void saveDay(const core::daylog::Day& d) {
  if (maps::sdStatus().state != maps::SdState::Mounted || d.date == 0) return;
  mkdir(sdPath("log").c_str(), 0775);  // EEXIST is fine
  writeText(("log/" + core::daylog::fileName(d.date)).c_str(), core::daylog::serialize(d));
}

/** Network task, each fetch: counts what's near home today. */
void dayLogObserve(const std::vector<core::Aircraft>& all) {
  if (!plat::timeValid()) return;
  const std::tm t = core::localTime(plat::unixTimeMs() / 1000);
  const int date = core::daylog::dateOf(t);
  core::daylog::Day finished;
  {
    std::lock_guard<std::mutex> lock(s_day_mu);
    if (s_day.date != date) {
      finished = s_day;  // yesterday (or nothing yet)
      s_day_loaded = false;
    }
  }
  if (finished.date != 0) saveDay(finished);
  bool load = false;
  {
    std::lock_guard<std::mutex> lock(s_day_mu);
    load = !s_day_loaded;
    s_day_loaded = true;
  }
  if (load) {
    // After a restart, carry on with today's file.
    std::string text;
    core::daylog::Day from_file;
    if (maps::sdStatus().state == maps::SdState::Mounted && readText(("log/" + core::daylog::fileName(date)).c_str(), &text) &&
        core::daylog::parse(text, &from_file) && from_file.date == date) {
      std::lock_guard<std::mutex> lock(s_day_mu);
      s_day = from_file;
      std::printf("daylog: continuing %s (%d aircraft)\n", core::daylog::fileName(date).c_str(), s_day.count());
    }
  }
  std::lock_guard<std::mutex> lock(s_day_mu);
  core::daylog::observe(&s_day, all, {s_settings.home_lat, s_settings.home_lon}, s_settings.log_radius_km, date,
                        t.tm_hour * 60 + t.tm_min, &isFavorite);
  s_day_dirty = true;
}

/** Network task: saves today's log now and then. True if it wrote. */
bool serviceDayLog() {
  core::daylog::Day copy;
  {
    std::lock_guard<std::mutex> lock(s_day_mu);
    if (!s_day_dirty || plat::millis() - s_day_saved_ms < kDaySaveEveryMs || s_day.date == 0) return false;
    s_day_dirty = false;
    s_day_saved_ms = plat::millis();
    copy = s_day;
  }
  saveDay(copy);
  return true;
}

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
  applySettings(next);
  {
    std::lock_guard<std::mutex> lock(s_mu);
    s_notices.push_back(problems.empty() ? "Settings loaded from SD card"
                                         : "Settings loaded from SD card (some lines ignored)");
  }
}

/** UI task: replaces the settings, applying whatever needs more than a redraw. */
void applySettings(const core::Settings& next) {
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
  for (const std::string& host : core::trace::hostsFor(s_failover.active())) {
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

/** Local minute of the day for a unix time. */
int localMinute(int64_t unix_s) {
  const std::tm t = core::localTime(unix_s);
  return t.tm_hour * 60 + t.tm_min;
}

/** Brightness while in use: the normal one, or the night one. */
int normalBrightness() {
  return isNight() && !s_settings.night_sleep ? s_settings.night_brightness : s_settings.brightness;
}

void applyHardware() {
  if (s_sleeping) plat::setBrightness(0);
  else if (!s_dimmed) plat::setBrightness(normalBrightness());
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

/** Returns whether the response held usable traffic. */
bool ingestImpl(int code, const std::string& body, std::vector<core::Aircraft>* parsed,
                core::DataSource source = core::DataSource::AdsbFi) {
  // Parse everything, then apply the display filters here so favorites can
  // bypass them (a saved aircraft is never hidden, and still raises alerts).
  core::adsb::ParseOptions opt;
  opt.include_ground = true;
  opt.min_alt_ft = -100000;
  opt.max_alt_ft = 1000000;
  const uint64_t parse_t0 = plat::micros();
  const bool ok = code == 200 && core::adsb::parse(body.data(), body.size(), opt, parsed);
  const uint32_t parse_us = static_cast<uint32_t>(plat::micros() - parse_t0);
  const int total = ok ? static_cast<int>(parsed->size()) : 0;
  if (ok) dayLogObserve(*parsed);  // everything near home, whatever the display filters
  if (ok) {
    const std::vector<core::Favorite> favs = favorites();
    const bool watching = core::watchActive(s_settings);
    parsed->erase(std::remove_if(parsed->begin(), parsed->end(),
                                 [&](const core::Aircraft& a) {
                                   return !core::passesFilters(a, s_settings) && !core::containsHex(favs, a.hex) &&
                                          !(watching && !core::watchReason(a, s_settings).empty());
                                 }),
                  parsed->end());
  }

  {
    std::lock_guard<std::mutex> lock(s_mu);
    s_feed.last_http = code;
    s_feed.last_ok = ok;
    s_feed.source = source;
    if (ok) s_feed.parse_us = parse_us;
    if (ok) {
      s_feed.ever_ok = true;
      s_feed.last_ok_ms = plat::millis();
      s_feed.last_count = static_cast<int>(parsed->size());
      s_feed.last_total = total;
      s_feed.error[0] = '\0';
    } else if (code == 200) {
      std::snprintf(s_feed.error, sizeof(s_feed.error), "Bad data from %s", core::adsb::sourceName(source));
    } else if (code == 429) {
      std::snprintf(s_feed.error, sizeof(s_feed.error), "Rate limited - slow down polling");
    } else if (code < 0) {
      std::snprintf(s_feed.error, sizeof(s_feed.error), "Network error (%d)", code);
    } else {
      std::snprintf(s_feed.error, sizeof(s_feed.error), "HTTP %d", code);
    }
  }
  if (!ok) return false;

  core::AlertZone zone;
  zone.center = {s_settings.home_lat, s_settings.home_lon};
  zone.radius_km = s_settings.alert_radius_km;
  zone.max_alt_ft = s_settings.alert_max_alt_ft;
  const std::vector<core::Favorite> favs = favorites();
  core::FavoriteWatch watch;
  watch.favorites = &favs;
  const std::vector<core::Favorite> quiet = silenced();
  watch.silenced = &quiet;
  watch.center = zone.center;
  watch.near_radius_km = s_settings.favorite_near_radius_km;
  if (core::watchActive(s_settings)) {
    watch.watch = [](const core::Aircraft& a) { return core::watchReason(a, s_settings); };
    watch.watch_radius_km = s_settings.watch_radius_km;
  }
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
    for (const core::WatchHit& w : ev.watch_hits) {
      char dist[24];
      formatDistance(w.km, dist, sizeof(dist));
      std::string what = w.reason;
      if (!w.type.empty() && w.type != w.reason && w.reason != w.name) what = w.type + ", " + w.reason;
      else if (w.reason == w.name) what = w.type.empty() ? "" : w.type;
      s_notices.push_back("Watchlist: " + w.name + (what.empty() ? "" : " (" + what + ")") + ", " + dist);
    }
    while (s_notices.size() > 4) s_notices.pop_front();
  }

  // One sound per fetch, most important first.
  if (ev.new_emergencies > 0) {
    sound(core::Sound::Emergency);
  } else if (!ev.watch_hits.empty() && s_settings.sound_watch) {
    sound(core::Sound::Watch);
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
  return true;
}

/** Creates the directories on the way to a file under the card ("a/b/c.txt" makes a and a/b). */
void makeDirsFor(const std::string& rel) {
  for (size_t p = rel.find('/'); p != std::string::npos; p = rel.find('/', p + 1)) {
    mkdir(sdPath(rel.substr(0, p).c_str()).c_str(), 0775);  // EEXIST is fine
  }
}

bool cardReady() { return maps::sdStatus().state == maps::SdState::Mounted; }

int today() { return plat::timeValid() ? core::daylog::dateOf(core::localTime(plat::unixTimeMs() / 1000)) : 0; }

// Lookups kept on the card. Guarded by s_route_mu (with the route state).
bool s_route_disk_checked = false;  // the card was asked about the pending lookup
std::atomic<bool> s_lookup_clear{false};

/** Network task: answers a pending lookup from the card when it has a fresh copy. True if it read the card. */
bool serviceRouteCache() {
  char hex[8], callsign[9];
  {
    std::lock_guard<std::mutex> lock(s_route_mu);
    if (!s_route_pending || s_route_disk_checked) return false;
    s_route_disk_checked = true;
    if (!s_settings.lookup_cache || !cardReady() || !plat::timeValid()) return false;
    std::memcpy(hex, s_route.hex, sizeof(hex));
    std::memcpy(callsign, s_route.callsign, sizeof(callsign));
  }
  const std::string af = core::route::airframePath(hex);
  if (af.empty()) return false;
  const int now = today();
  core::route::Info info;
  std::string text;
  int date = 0;
  bool empty = true;
  if (!readText(af.c_str(), &text) || !core::route::parseCached(text, &info, &date, &empty) ||
      !core::route::airframeFresh(date, now, empty)) {
    return true;
  }
  if (callsign[0] != '\0') {
    const std::string rp = core::route::routePath(callsign);
    bool route_empty = true;
    if (rp.empty() || !readText(rp.c_str(), &text) || !core::route::parseCached(text, &info, &date, &route_empty) ||
        !core::route::routeFresh(date, now, route_empty)) {
      return true;
    }
  }
  std::lock_guard<std::mutex> lock(s_route_mu);
  RouteState done;
  std::memcpy(done.hex, hex, sizeof(hex));
  std::memcpy(done.callsign, callsign, sizeof(callsign));
  done.info = info;
  const bool any = info.airline[0] || info.model[0] || info.owner[0] || info.has_route;
  done.status = any ? RouteStatus::Ready : RouteStatus::Unknown;
  s_route_cache.push_front(done);
  if (s_route_cache.size() > kRouteCacheSize) s_route_cache.pop_back();
  if (sameKey(s_route, hex, callsign) && s_route_pending) {
    s_route = done;
    s_route_pending = false;
  }
  std::printf("route: %s %s from the SD card\n", hex, callsign);
  return true;
}

/** Network task: keeps a finished lookup on the card. */
void saveLookup(const char* hex, const char* callsign, const core::route::Info& info, bool airframe_known) {
  if (!s_settings.lookup_cache || !cardReady() || !plat::timeValid()) return;
  const int now = today();
  const std::string af = core::route::airframePath(hex);
  if (!af.empty() && airframe_known) {
    makeDirsFor(af);
    writeText(af.c_str(), core::route::serializeAirframe(info, now));
  }
  const std::string rp = core::route::routePath(callsign);
  if (!rp.empty()) {
    makeDirsFor(rp);
    writeText(rp.c_str(), core::route::serializeRoute(info, now));
  }
}

/** Deletes a directory and everything in it. */
void removeTree(const std::string& path) {
  DIR* d = opendir(path.c_str());
  if (d == nullptr) {
    std::remove(path.c_str());
    return;
  }
  std::vector<std::string> names;
  while (dirent* e = readdir(d)) {
    if (std::strcmp(e->d_name, ".") != 0 && std::strcmp(e->d_name, "..") != 0) names.push_back(e->d_name);
  }
  closedir(d);
  for (const std::string& n : names) removeTree(path + "/" + n);
  rmdir(path.c_str());
}

bool serviceLookupClear() {
  if (!s_lookup_clear.exchange(false) || !cardReady()) return false;
  removeTree(sdPath("lookups"));
  std::lock_guard<std::mutex> lock(s_mu);
  s_notices.push_back("Saved lookups deleted");
  return true;
}

// Screenshots: taken on the UI task, compressed and written here.
std::mutex s_shot_mu;
std::vector<uint8_t> s_shot_px;
int s_shot_w = 0, s_shot_h = 0, s_shot_stride = 0;
std::string s_shot_name;
std::function<bool()> s_take_screenshot;  // UI task

bool serviceScreenshot() {
  std::vector<uint8_t> px;
  std::string name;
  int w, h, stride;
  {
    std::lock_guard<std::mutex> lock(s_shot_mu);
    if (s_shot_name.empty()) return false;
    px.swap(s_shot_px);
    name = s_shot_name;
    w = s_shot_w;
    h = s_shot_h;
    stride = s_shot_stride;
  }
  const uint32_t t0 = plat::millis();
  const std::string png = core::png::encodeRgb(px.data(), w, h, stride, true);
  px.clear();
  px.shrink_to_fit();
  const std::string rel = "screenshots/" + name;
  makeDirsFor(rel);
  writeText(rel.c_str(), png);
  std::printf("screenshot: %s, %u KB in %u ms\n", name.c_str(), static_cast<unsigned>(png.size() / 1024),
              static_cast<unsigned>(plat::millis() - t0));
  {
    std::lock_guard<std::mutex> lock(s_shot_mu);
    s_shot_name.clear();
  }
  std::lock_guard<std::mutex> lock(s_mu);
  s_notices.push_back("Screenshot saved: " + rel);
  return true;
}

// Work handed to the UI task by other tasks (see runOnUi).
struct UiJob {
  std::mutex mu;
  std::condition_variable cv;
  bool done = false, cancelled = false;
  std::function<void()> fn;
};
std::mutex s_ui_jobs_mu;
std::deque<std::shared_ptr<UiJob>> s_ui_jobs;

void runUiJobs() {
  std::deque<std::shared_ptr<UiJob>> jobs;
  {
    std::lock_guard<std::mutex> lock(s_ui_jobs_mu);
    jobs.swap(s_ui_jobs);
  }
  for (auto& j : jobs) {
    {
      std::lock_guard<std::mutex> lock(j->mu);
      if (j->cancelled) continue;
      j->fn();
      j->done = true;
    }
    j->cv.notify_all();
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
    if (maps::serviceSd() || serviceSdFiles() || serviceDayLog() || serviceCardWork()) continue;
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
    s_failover.setPreferred(s_settings.source);
    const core::DataSource source = s_failover.next(now);
    const std::string url = core::adsb::buildUrl(source, center.lat, center.lon, radius_km * 1.25 + 5.0);

    // Kept open between updates: no new TLS handshake every few seconds.
    const int code = plat::httpGetKeepAlive(url.c_str(), &body, 10000);
    const bool ok = ingestImpl(code, body, &parsed, source);
    const core::SourceFailover::Change change = s_failover.report(ok, plat::millis());
    if (change != core::SourceFailover::Change::None) {
      const std::string text =
          change == core::SourceFailover::Change::Switched
              ? std::string(core::adsb::sourceName(source)) + " isn't answering - using " +
                    core::adsb::sourceName(s_failover.active())
              : std::string("Back on ") + core::adsb::sourceName(source);
      std::printf("feed: %s\n", text.c_str());
      std::lock_guard<std::mutex> lock(s_mu);
      s_notices.push_back(text);
    }
  }
}

}  // namespace

void init() {
  plat::settingsLoad(&s_settings);
  plat::favoritesLoad(&s_favorites);
  plat::silencedLoad(&s_silenced);
  s_query_center = {s_settings.home_lat, s_settings.home_lon};
  s_query_radius_km = s_settings.home_radius_km;
  s_last_activity = plat::millis();
  core::applyTimeZone(s_settings.time_zone);
  applyHardware();
  if (s_settings.wifi_ssid[0] != '\0') {
    plat::wifiConnect(s_settings.wifi_ssid, s_settings.wifi_pass);
  }
  const plat::BootInfo boot = plat::bootInfo();
  if (boot.rolled_back_from[0] != '\0') {
    std::lock_guard<std::mutex> lock(s_mu);
    s_notices.push_back(std::string("Went back to ") + plat::firmwareVersion() + ": " + boot.rolled_back_from +
                        " restarted 3 times in a row");
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

core::daylog::Day dayLogToday() {
  std::lock_guard<std::mutex> lock(s_day_mu);
  return s_day;
}

std::vector<core::daylog::Day> dayLogHistory(int days) {
  std::vector<core::daylog::Day> out;
  if (maps::sdStatus().state != maps::SdState::Mounted || !plat::timeValid()) return out;
  int64_t t = plat::unixTimeMs() / 1000;
  for (int i = 1; i <= days; ++i) {
    const int date = core::daylog::dateOf(core::localTime(t - static_cast<int64_t>(i) * 86400));
    std::string text;
    core::daylog::Day d;
    if (readText(("log/" + core::daylog::fileName(date)).c_str(), &text) && core::daylog::parse(text, &d)) {
      out.push_back(d);
    }
  }
  return out;
}

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
  static bool s_confirmed = false, s_stable = false;
  if (!s_stable && now_ms > 20000) {
    s_stable = true;
    plat::bootStable();  // not a "quick" restart any more (see plat::bootInfo)
  }
  if (!s_confirmed && now_ms > 60000) {
    s_confirmed = true;
    plat::confirmFirmware();
  }
  runUiJobs();
  applySettingsFile();
  serviceWifi(now_ms);
  web::service();
  // Night mode starts and ends by the clock: dimmer, or asleep.
  static uint32_t s_night_check_ms = 0;
  static int s_night = -1;
  static bool s_night_slept = false;  // asleep because of night mode (so the morning wakes it)
  constexpr uint32_t kNightResleepMs = 60000;  // woken by a tap at night: back to sleep after this idle
  if (now_ms - s_night_check_ms >= 5000) {
    s_night_check_ms = now_ms;
    const int night = isNight() ? 1 : 0;
    if (night != s_night) {
      const bool first = s_night < 0;  // just started: don't put it to sleep under someone's fingers
      s_night = night;
      if (s_settings.night_sleep && night && !first && !s_sleeping) {
        setSleeping(true);
        s_night_slept = true;
      } else if (!night && s_night_slept) {
        if (s_sleeping) setSleeping(false);
        s_night_slept = false;
      }
      applyHardware();
    }
    if (night && s_settings.night_sleep && !s_sleeping && s_ringing < 0 &&
        now_ms - s_last_activity >= kNightResleepMs) {
      setSleeping(true);
      s_night_slept = true;
    }
  }
  checkAlarms(now_ms);
  if (s_dirty && now_ms - s_dirty_since >= (s_menu_open.load() ? kSaveInMenuMs : kSaveDebounceMs)) settingsFlush();

  if (s_settings.dim_after_min > 0 && !s_dimmed && !s_sleeping &&
      now_ms - s_last_activity >= s_settings.dim_after_min * 60000u) {
    s_dimmed = true;
    plat::setBrightness(std::min<int>(s_settings.dim_brightness, normalBrightness()));
  }
}

bool isNight() {
  if (!s_settings.night_dim || !plat::timeValid()) return false;
  const int now = localMinute(plat::unixTimeMs() / 1000);
  if (s_settings.night_by_sun) {
    int set = 0, rise = 0;
    bool dark = false;
    if (!sunTimesToday(&set, &rise, &dark)) return dark;
    return core::inTimeWindow(now, set, rise);
  }
  return core::inTimeWindow(now, s_settings.night_from_min, s_settings.night_to_min);
}

bool sunTimesToday(int* sunset_min, int* sunrise_min, bool* always_dark) {
  if (always_dark) *always_dark = false;
  if (!plat::timeValid()) return false;
  const std::tm t = core::localTime(plat::unixTimeMs() / 1000);
  const core::sun::Times st =
      core::sun::times(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, s_settings.home_lat, s_settings.home_lon);
  if (st.kind != core::sun::Times::Kind::Normal) {
    if (always_dark) *always_dark = st.kind == core::sun::Times::Kind::AlwaysDown;
    return false;
  }
  *sunset_min = localMinute(st.set_s);
  *sunrise_min = localMinute(st.rise_s);
  return true;
}

int applySettingsText(const std::string& text, std::vector<std::string>* problems) {
  core::Settings next = s_settings;
  const int n = core::settings_text::parse(text, &next, problems);
  if (std::memcmp(&next, &s_settings, sizeof(next)) != 0) applySettings(next);
  return n;
}

bool runOnUi(std::function<void()> fn, uint32_t timeout_ms) {
  auto job = std::make_shared<UiJob>();
  job->fn = std::move(fn);
  {
    std::lock_guard<std::mutex> lock(s_ui_jobs_mu);
    s_ui_jobs.push_back(job);
  }
  std::unique_lock<std::mutex> lock(job->mu);
  if (job->cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&] { return job->done; })) return true;
  job->cancelled = true;
  return false;
}

bool saveScreenshot(std::vector<uint8_t> bgr, int width, int height, int stride) {
  if (!cardReady()) return false;
  std::lock_guard<std::mutex> lock(s_shot_mu);
  if (!s_shot_name.empty()) return false;  // still writing the last one
  char name[40];
  if (plat::timeValid()) {
    const std::tm t = core::localTime(plat::unixTimeMs() / 1000);
    std::snprintf(name, sizeof(name), "%04d-%02d-%02d_%02d-%02d-%02d.png", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                  t.tm_hour, t.tm_min, t.tm_sec);
  } else {
    std::snprintf(name, sizeof(name), "screen-%08u.png", static_cast<unsigned>(plat::millis()));
  }
  s_shot_px = std::move(bgr);
  s_shot_w = width;
  s_shot_h = height;
  s_shot_stride = stride;
  s_shot_name = name;
  return true;
}

void onScreenshotRequest(std::function<bool()> take) { s_take_screenshot = std::move(take); }
bool requestScreenshot() { return s_take_screenshot && s_take_screenshot(); }

std::vector<std::string> screenshots() {
  std::vector<std::string> out;
  if (!cardReady()) return out;
  DIR* d = opendir(sdPath("screenshots").c_str());
  if (d == nullptr) return out;
  while (dirent* e = readdir(d)) {
    const std::string n = e->d_name;
    if (!screenshotPath(n).empty()) out.push_back(n);
  }
  closedir(d);
  std::sort(out.rbegin(), out.rend());  // names are dates: newest first
  if (out.size() > 60) out.resize(60);
  return out;
}

std::string screenshotPath(const std::string& name) {
  if (name.size() < 5 || name.size() > 40 || name.compare(name.size() - 4, 4, ".png") != 0) return "";
  for (char c : name) {
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_' && c != '.') return "";
  }
  if (name.find("..") != std::string::npos) return "";
  return sdPath("screenshots") + "/" + name;
}

void clearLookupCache() { s_lookup_clear = true; }

bool serviceCardWork() { return serviceScreenshot() || serviceRouteCache() || serviceLookupClear(); }

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

void setMenuOpen(bool open) {
  s_menu_open = open;
  if (!open) settingsFlush();  // changes made in the menu
}

bool userActivity() {
  s_last_activity = plat::millis();
  s_last_touch_ms = s_last_activity;
  if (s_sleeping || !s_dimmed) return false;  // waking from sleep is the UI's job (one tap)
  s_dimmed = false;
  plat::setBrightness(normalBrightness());
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

std::vector<core::Favorite> silenced() {
  std::lock_guard<std::mutex> lock(s_favorites_mu);
  return s_silenced;
}

bool isSilenced(const char* hex) {
  std::lock_guard<std::mutex> lock(s_favorites_mu);
  return core::containsHex(s_silenced, hex);
}

bool toggleSilenced(const core::Aircraft& a) {
  bool now_silenced;
  bool was_favorite = false;
  std::vector<core::Favorite> copy, favs;
  {
    std::lock_guard<std::mutex> lock(s_favorites_mu);
    auto it = std::find_if(s_silenced.begin(), s_silenced.end(),
                           [&](const core::Favorite& f) { return std::strcmp(f.hex, a.hex) == 0; });
    if (it != s_silenced.end()) {
      s_silenced.erase(it);
      now_silenced = false;
    } else {
      if (s_silenced.size() >= core::kMaxFavorites) return false;
      core::Favorite f;
      std::snprintf(f.hex, sizeof(f.hex), "%s", a.hex);
      std::snprintf(f.label, sizeof(f.label), "%s", a.callsign[0] ? a.callsign : (a.reg[0] ? a.reg : a.hex));
      std::snprintf(f.type, sizeof(f.type), "%s", a.type);
      s_silenced.push_back(f);
      now_silenced = true;
      // Silenced and favorite exclude each other.
      const size_t n = s_favorites.size();
      s_favorites.erase(std::remove_if(s_favorites.begin(), s_favorites.end(),
                                       [&](const core::Favorite& x) { return std::strcmp(x.hex, a.hex) == 0; }),
                        s_favorites.end());
      was_favorite = s_favorites.size() != n;
      favs = s_favorites;
    }
    copy = s_silenced;
  }
  plat::silencedSave(copy);
  if (was_favorite) {
    plat::favoritesSave(favs);
    queueFavoritesFile();
  }
  return now_silenced;
}

void removeSilenced(const char* hex) {
  std::vector<core::Favorite> copy;
  {
    std::lock_guard<std::mutex> lock(s_favorites_mu);
    s_silenced.erase(std::remove_if(s_silenced.begin(), s_silenced.end(),
                                    [&](const core::Favorite& f) { return std::strcmp(f.hex, hex) == 0; }),
                     s_silenced.end());
    copy = s_silenced;
  }
  plat::silencedSave(copy);
}

bool toggleFavorite(const core::Aircraft& a) {
  bool now_favorite;
  std::vector<core::Favorite> copy;
  std::optional<std::vector<core::Favorite>> quiet;  // the silenced list, if it changed
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
      const size_t n = s_silenced.size();  // a favorite isn't silenced
      s_silenced.erase(std::remove_if(s_silenced.begin(), s_silenced.end(),
                                      [&](const core::Favorite& x) { return std::strcmp(x.hex, a.hex) == 0; }),
                       s_silenced.end());
      if (s_silenced.size() != n) quiet = s_silenced;
    }
    copy = s_favorites;
  }
  plat::favoritesSave(copy);
  if (quiet) plat::silencedSave(*quiet);
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

void renameFavorite(const char* hex, const char* label) {
  std::vector<core::Favorite> copy;
  {
    std::lock_guard<std::mutex> lock(s_favorites_mu);
    for (core::Favorite& f : s_favorites) {
      if (std::strcmp(f.hex, hex) == 0) std::snprintf(f.label, sizeof(f.label), "%s", label);
    }
    copy = s_favorites;
  }
  plat::favoritesSave(copy);
  queueFavoritesFile();
}

std::vector<std::string> unresolvedFavorites() {
  std::lock_guard<std::mutex> lock(s_files_mu);
  return s_fav_unresolved;
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
  ingestImpl(http_code, body, &parsed, s_settings.source);
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
  s_route_disk_checked = false;
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
  if (reached) saveLookup(hex, callsign, info, code == 200 || code == 404);

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
