#include "app/app.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <iterator>
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
#include "core/backup.h"
#include "core/failover.h"
#include "core/filter.h"
#include "app/maps.h"
#include "app/web.h"
#include "app/wifi_internal.h"
#include "core/inflate.h"
#include "core/job_queue.h"
#include "core/net_watch.h"
#include "core/wav.h"
#include "core/logbook.h"
#include "core/lookup_cache.h"
#include "core/png.h"
#include "core/sun.h"
#include "core/watch.h"
#include "core/replay.h"
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
// menu, where one change follows another, nothing is saved until it's left
// (a periodic save there was a freeze in the middle of scrolling). Changes
// that can't wait, like a new Wi-Fi network, flush themselves.

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

// SD card writes run on their own low-priority task ("card"), in order, so
// the network task never waits for the card (a slow write used to hold up
// the feed and the map, and the UI when it read the card at the same time).
core::JobQueue s_card(4 * 1024 * 1024);  // at most this much data waiting (tile cache writes beyond it are dropped)

/** Card task: writes a file (temporary file, then rename: a power cut never leaves half a file). */
void writeTextNow(const std::string& name, const std::string& text) {
  for (size_t p = name.find('/'); p != std::string::npos; p = name.find('/', p + 1)) {
    mkdir(sdPath(name.substr(0, p).c_str()).c_str(), 0775);  // the folders on the way; EEXIST is fine
  }
  const std::string path = sdPath(name.c_str()), tmp = path + ".tmp";
  FILE* f = std::fopen(tmp.c_str(), "wb");
  bool ok = f != nullptr && std::fwrite(text.data(), 1, text.size(), f) == text.size();
  if (f != nullptr && std::fclose(f) != 0) ok = false;
  if (ok) {
    std::remove(path.c_str());
    ok = std::rename(tmp.c_str(), path.c_str()) == 0;
  } else {
    std::remove(tmp.c_str());
  }
  std::printf("sd: %s %s (%u bytes)\n", ok ? "wrote" : "FAILED to write", name.c_str(),
              static_cast<unsigned>(text.size()));
}

/** Any task: writes a file on the card task (the folders on the way are made there too). */
void writeText(const char* name, const std::string& text) {
  std::string n = name;
  const size_t bytes = text.size();
  s_card.push([n, text] { writeTextNow(n, text); }, bytes, false);
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
  writeText(("log/" + core::daylog::fileName(d.date)).c_str(), core::daylog::serialize(d));
}

// Logbook (logbook.csv on the card). Network task, except the UI's read-only
// queries, which lock s_book_mu.
constexpr char kLogbookFile[] = "logbook.csv";
std::mutex s_book_mu;
core::logbook::Book s_book;
std::atomic<bool> s_book_loaded{false};  // read from the card (set by the card task)
bool s_book_load_queued = false;          // network task      // read from the card (or found missing)
std::string s_book_pending;      // CSV lines not on the card yet
uint32_t s_book_saved_ms = 0;
std::vector<core::logbook::Entry> s_book_firsts;  // this fetch's first-timers (network task)

/** Network task: reads logbook.csv into the index once the card is there. */
void loadLogbookNow();

/** Network task: has the card task read logbook.csv into the index (once the card is there). */
void loadLogbook() {
  if (s_book_load_queued || maps::sdStatus().state != maps::SdState::Mounted) return;
  s_book_load_queued = true;
  s_card.push(loadLogbookNow, 0, false);
}

/** Card task: reads logbook.csv (it can be large: off the network task). */
void loadLogbookNow() {
  core::logbook::Book book;
  FILE* f = std::fopen(sdPath(kLogbookFile).c_str(), "rb");
  size_t lines = 0;
  if (f != nullptr) {
    std::string line;
    char buf[1024];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
      for (size_t i = 0; i < n; ++i) {
        if (buf[i] == '\n') {
          book.loadLine(line);
          line.clear();
          ++lines;
        } else {
          line.push_back(buf[i]);
        }
      }
    }
    if (!line.empty()) book.loadLine(line);
    std::fclose(f);
  }
  book.loaded();
  std::printf("logbook: %u aircraft, %u visits since %d (%u lines)\n", static_cast<unsigned>(book.aircraft()),
              static_cast<unsigned>(book.visits()), book.startDate(), static_cast<unsigned>(lines));
  std::lock_guard<std::mutex> lock(s_book_mu);
  std::swap(s_book, book);
  s_book_loaded = true;
}

/** Network task: appends new visits to logbook.csv now and then. True if it wrote. */
bool serviceLogbook(bool now = false) {
  std::string text;
  {
    std::lock_guard<std::mutex> lock(s_book_mu);
    if (!s_book_loaded || s_book_pending.empty()) return false;
    if (!now && plat::millis() - s_book_saved_ms < 60000 && s_book_pending.size() < 4096) return false;
    text.swap(s_book_pending);
    s_book_saved_ms = plat::millis();
  }
  if (maps::sdStatus().state != maps::SdState::Mounted) return false;
  const size_t bytes = text.size();
  s_card.push([text] {
    const std::string path = sdPath(kLogbookFile);
    struct stat st;
    const bool fresh = stat(path.c_str(), &st) != 0;
    FILE* f = std::fopen(path.c_str(), "ab");
    if (f == nullptr) return;
    if (fresh) std::fputs(core::logbook::csvHeader(), f);
    std::fwrite(text.data(), 1, text.size(), f);
    std::fclose(f);
  }, bytes, false);
  return true;
}

/** Network task, each fetch: the logbook's visits, and who's here for the first time. */
void logbookObserve(const std::vector<core::Aircraft>& all, const std::tm& t, int date) {
  s_book_firsts.clear();
  loadLogbook();
  if (!s_book_loaded) return;  // no card (yet): nothing to compare with
  std::vector<core::Aircraft> near;
  const core::LatLon home{s_settings.home_lat, s_settings.home_lon};
  for (const core::Aircraft& a : all) {
    if (!a.on_ground && core::distanceKm(home, {a.lat, a.lon}) <= s_settings.log_radius_km) near.push_back(a);
  }
  std::lock_guard<std::mutex> lock(s_book_mu);
  const bool learning = s_book.learning(date);
  std::vector<core::logbook::Entry> firsts =
      s_book.observe(near, plat::unixTimeMs() / 1000, date, t.tm_hour * 100 + t.tm_min, &s_book_pending);
  if (!learning) s_book_firsts = std::move(firsts);
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
  logbookObserve(all, t, date);
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

// Flight replay: recording (network task) and playing back (network task,
// steered from the UI through s_replay_mu).
int64_t s_replay_recorded_s = 0;
int s_replay_pruned_date = 0;
constexpr double kReplayRadiusKm = 150.0;

/** Network task, each fetch: a snapshot of the traffic around home, every kFrameEveryS. */
void recordReplay(const std::vector<core::Aircraft>& all) {
  if (!s_settings.replay_record || !plat::timeValid() || maps::sdStatus().state != maps::SdState::Mounted) return;
  const int64_t now = plat::unixTimeMs() / 1000;
  if (now - s_replay_recorded_s < core::replay::kFrameEveryS) return;
  s_replay_recorded_s = now;
  const core::LatLon home{s_settings.home_lat, s_settings.home_lon};
  std::vector<core::Aircraft> near;
  for (const core::Aircraft& a : all) {
    if (near.size() < core::TrafficStore::kMaxTracks && core::distanceKm(home, {a.lat, a.lon}) <= kReplayRadiusKm) {
      near.push_back(a);
    }
  }
  const int date = core::daylog::dateOf(core::localTime(now));
  const bool new_day = date != s_replay_pruned_date;
  s_replay_pruned_date = date;
  std::string frame = core::replay::encodeFrame(now, near);
  const size_t bytes = frame.size();
  s_card.push([date, new_day, frame] {
    mkdir(sdPath("replay").c_str(), 0775);
    if (new_day) {
      // A new day: the oldest days go.
      const int oldest = core::replay::addDays(date, -(core::replay::kKeepDays - 1));
      if (DIR* d = opendir(sdPath("replay").c_str())) {
        std::vector<std::string> old;
        while (dirent* e = readdir(d)) {
          const int fd = core::replay::dateOfFile(e->d_name);
          if (fd != 0 && fd < oldest) old.push_back(e->d_name);
        }
        closedir(d);
        for (const std::string& n : old) std::remove((sdPath("replay") + "/" + n).c_str());
      }
    }
    if (FILE* f = std::fopen(sdPath(core::replay::fileName(date).c_str()).c_str(), "ab")) {
      std::fwrite(frame.data(), 1, frame.size(), f);
      std::fclose(f);
    }
  }, bytes, false);
}

struct ReplayCtl {
  bool active = false;
  int date = 0;
  int64_t pos_s = 0;      // the moment shown
  double pos = 0;         // the same, with the fraction
  bool playing = false;
  int speed = 10;
  uint32_t tick_ms = 0;   // when pos_s last moved on
  std::vector<std::pair<int64_t, uint32_t>> index;  // frame time, file offset
  int loaded_date = 0;
  int shown = -1;         // index of the frame in the traffic
  uint32_t shown_ms = 0;
};
std::mutex s_replay_mu;
ReplayCtl s_replay;

/** Network task: plays the replay on. True if it read a frame. */
bool serviceReplayImpl() {
  int date;
  {
    std::lock_guard<std::mutex> lock(s_replay_mu);
    if (!s_replay.active) return false;
    date = s_replay.date;
    if (s_replay.loaded_date == date) date = 0;
  }
  if (date != 0) {
    // The day's frames: where each one starts.
    std::vector<std::pair<int64_t, uint32_t>> index;
    if (FILE* f = std::fopen(sdPath(core::replay::fileName(date).c_str()).c_str(), "rb")) {
      uint8_t head[core::replay::kHeaderBytes];
      long off = 0;
      while (std::fread(head, 1, sizeof(head), f) == sizeof(head)) {
        core::replay::FrameHeader h;
        if (!core::replay::readHeader(head, &h)) break;
        index.emplace_back(h.time_s, static_cast<uint32_t>(off));
        off += static_cast<long>(sizeof(head) + h.packed);
        if (std::fseek(f, off, SEEK_SET) != 0) break;
      }
      std::fclose(f);
    }
    std::printf("replay: %d has %u frames\n", date, static_cast<unsigned>(index.size()));
    std::lock_guard<std::mutex> lock(s_replay_mu);
    s_replay.index = std::move(index);
    s_replay.loaded_date = date;
    s_replay.shown = -1;
    if (!s_replay.index.empty() &&
        (s_replay.pos_s < s_replay.index.front().first || s_replay.pos_s > s_replay.index.back().first)) {
      s_replay.pos_s = s_replay.index.front().first;
    }
    s_replay.pos = static_cast<double>(s_replay.pos_s);
  }
  int want;
  uint32_t offset;
  int load_date;
  {
    std::lock_guard<std::mutex> lock(s_replay_mu);
    ReplayCtl& r = s_replay;
    if (r.index.empty()) return false;
    const uint32_t now = plat::millis();
    if (r.playing) {
      r.pos += r.speed * (now - r.tick_ms) / 1000.0;
      if (r.pos >= r.index.back().first) {
        r.pos = static_cast<double>(r.index.back().first);
        r.playing = false;
      }
    }
    r.tick_ms = now;
    r.pos_s = static_cast<int64_t>(r.pos);
    auto it = std::upper_bound(r.index.begin(), r.index.end(), r.pos_s,
                               [](int64_t t, const std::pair<int64_t, uint32_t>& e) { return t < e.first; });
    want = it == r.index.begin() ? 0 : static_cast<int>(it - r.index.begin()) - 1;
    // The same frame again now and then, so paused aircraft don't time out.
    if (want == r.shown && now - r.shown_ms < 10000) return false;
    offset = r.index[static_cast<size_t>(want)].second;
    load_date = r.loaded_date;
  }
  std::vector<core::Aircraft> frame;
  if (FILE* f = std::fopen(sdPath(core::replay::fileName(load_date).c_str()).c_str(), "rb")) {
    uint8_t head[core::replay::kHeaderBytes];
    core::replay::FrameHeader h;
    if (std::fseek(f, static_cast<long>(offset), SEEK_SET) == 0 && std::fread(head, 1, sizeof(head), f) == sizeof(head) &&
        core::replay::readHeader(head, &h)) {
      std::vector<uint8_t> packed(h.packed);
      if (std::fread(packed.data(), 1, packed.size(), f) == packed.size()) core::replay::decodeFrame(h, packed.data(), &frame);
    }
    std::fclose(f);
  }
  const std::vector<core::Favorite> favs = favorites();
  frame.erase(std::remove_if(frame.begin(), frame.end(),
                             [&](const core::Aircraft& a) {
                               return !core::passesFilters(a, s_settings) && !core::containsHex(favs, a.hex);
                             }),
              frame.end());
  {
    std::lock_guard<std::mutex> lock(s_replay_mu);
    if (!s_replay.active || s_replay.loaded_date != load_date) return true;  // stopped meanwhile
    s_replay.shown = want;
    s_replay.shown_ms = plat::millis();
  }
  core::AlertZone quiet;
  quiet.radius_km = 0;
  s_traffic.update(frame, plat::millis(), quiet);
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
  // New filters: start over, so aircraft they hide go now (and ones they show
  // come with the next update). Otherwise the aircraft stay on screen.
  const bool filters_changed =
      before.military_only != s_settings.military_only || before.show_ground != s_settings.show_ground ||
      before.min_alt_ft != s_settings.min_alt_ft || before.max_alt_ft != s_settings.max_alt_ft ||
      before.category_mask != s_settings.category_mask || before.min_speed_kt != s_settings.min_speed_kt ||
      before.max_speed_kt != s_settings.max_speed_kt || std::strcmp(before.type_filter, s_settings.type_filter) != 0;
  if (filters_changed) traffic().clear();
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
  if (ok) recordReplay(*parsed);
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
      std::snprintf(s_feed.error, sizeof(s_feed.error), "No connection");
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
    if (s_settings.first_time_alert) {
      for (const core::logbook::Entry& e : s_book_firsts) {
        char hex[8];
        std::snprintf(hex, sizeof(hex), "%06x", static_cast<unsigned>(e.hex));
        if (isSilenced(hex)) continue;
        s_notices.push_back(std::string("First time here: ") + e.label + (e.type[0] ? std::string(" (") + e.type + ")" : ""));
      }
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
  } else if (!s_book_firsts.empty() && s_settings.first_time_alert && s_settings.sound_first_time) {
    sound(core::Sound::FirstTime);
  } else if (ev.new_close_approaches > 0 && s_settings.sound_proximity) {
    sound(core::Sound::Alert);
  } else if (ev.new_aircraft > 0 && ev.new_aircraft <= 3) {
    // Bursts (first fetch after a big pan) would be noise, not information.
    sound(core::Sound::NewAircraft);
  }
  return true;
}

bool cardReady() { return maps::sdStatus().state == maps::SdState::Mounted; }

// Your own sounds: sounds/*.wav on the card replace the built-in ones. Read
// and decoded on the card task; the clips themselves live in PSRAM.
std::mutex s_sounds_mu;
std::vector<std::string> s_sounds_status;  // guarded by s_sounds_mu
bool s_sounds_custom[static_cast<int>(core::Sound::kCount)] = {};  // card task only
constexpr long kMaxSoundFile = 8L * 1024 * 1024;

/** Card task. */
void loadSoundsNow() {
  if (!cardReady()) return;
  std::vector<std::string> status;
  const int rate = plat::soundSampleRate();
  for (int i = 0; i < static_cast<int>(core::Sound::kCount); ++i) {
    const core::Sound snd = static_cast<core::Sound>(i);
    const char* name = core::wav::fileName(snd);
    if (name == nullptr) continue;
    auto builtIn = [&] {
      if (s_sounds_custom[i]) plat::setSoundClip(snd, {});  // gone from the card: the built-in one again
      s_sounds_custom[i] = false;
    };
    FILE* f = std::fopen(sdPath((std::string("sounds/") + name).c_str()).c_str(), "rb");
    if (f == nullptr) {
      builtIn();
      continue;
    }
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::string err;
    std::vector<int16_t> pcm;
    if (size <= 0 || size > kMaxSoundFile) {
      err = size <= 0 ? "empty" : "too big (8 MB at most)";
    } else {
      std::vector<uint8_t> data(static_cast<size_t>(size));  // PSRAM (large)
      if (std::fread(data.data(), 1, data.size(), f) != data.size()) {
        err = "couldn't be read";
      } else if (!core::wav::decode(data.data(), data.size(), rate, core::wav::maxSeconds(snd), &pcm, &err) ||
                 pcm.empty()) {
        if (err.empty()) err = "no sound in it";
        pcm.clear();
      }
    }
    std::fclose(f);
    if (!pcm.empty()) {
      char line[96];
      std::snprintf(line, sizeof(line), "%s: OK, %.1f s", name, static_cast<double>(pcm.size()) / rate);
      status.push_back(line);
      plat::setSoundClip(snd, std::move(pcm));
      s_sounds_custom[i] = true;
    } else {
      status.push_back(std::string(name) + ": " + err);
      builtIn();
    }
  }
  for (const std::string& l : status) std::printf("sounds: %s\n", l.c_str());
  std::lock_guard<std::mutex> lock(s_sounds_mu);
  s_sounds_status = std::move(status);
}

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
    writeText(af.c_str(), core::route::serializeAirframe(info, now));
  }
  const std::string rp = core::route::routePath(callsign);
  if (!rp.empty()) {
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
  s_card.push([] {
    removeTree(sdPath("lookups"));
    std::lock_guard<std::mutex> lock(s_mu);
    s_notices.push_back("Saved lookups deleted");
  }, 0, false);
  return true;
}

// Screenshots: taken on the UI task, compressed and written here.
std::mutex s_shot_mu;
std::vector<uint8_t> s_shot_px;
int s_shot_w = 0, s_shot_h = 0, s_shot_stride = 0;
std::string s_shot_name;
std::function<bool()> s_take_screenshot;  // UI task

bool s_shot_queued = false;  // network task: handed to the card task

void saveScreenshotNow(std::vector<uint8_t>& px, const std::string& name, int w, int h, int stride);

bool serviceScreenshot() {
  auto px = std::make_shared<std::vector<uint8_t>>();
  std::string name;
  int w, h, stride;
  {
    std::lock_guard<std::mutex> lock(s_shot_mu);
    if (s_shot_name.empty()) {
      s_shot_queued = false;
      return false;
    }
    if (s_shot_queued) return false;  // the card task is on it
    px->swap(s_shot_px);
    name = s_shot_name;
    w = s_shot_w;
    h = s_shot_h;
    stride = s_shot_stride;
  }
  s_shot_queued = true;
  const size_t bytes = px->size();
  s_card.push([px, name, w, h, stride] { saveScreenshotNow(*px, name, w, h, stride); }, bytes, false);
  return true;
}

/** Card task: compresses and writes a screenshot. */
void saveScreenshotNow(std::vector<uint8_t>& px, const std::string& name, int w, int h, int stride) {
  const uint32_t t0 = plat::millis();
  const std::string png = core::png::encodeRgb(px.data(), w, h, stride, true);
  px.clear();
  px.shrink_to_fit();
  const std::string rel = "screenshots/" + name;
  writeTextNow(rel, png);
  std::printf("screenshot: %s, %u KB in %u ms\n", name.c_str(), static_cast<unsigned>(png.size() / 1024),
              static_cast<unsigned>(plat::millis() - t0));
  {
    std::lock_guard<std::mutex> lock(s_shot_mu);
    s_shot_name.clear();
  }
  std::lock_guard<std::mutex> lock(s_mu);
  s_notices.push_back("Screenshot saved: " + rel);
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

// Wi-Fi can say "connected" while nothing gets through (seen with the link
// to the Wi-Fi chip stuck): rejoin the network, then restart.
core::NetWatch s_net_watch;

bool s_wifi_worked = false;  // connected at least once since the start

void netWatchAct(uint32_t now);

void watchNetwork(int code) {
  const uint32_t now = plat::millis();
  s_wifi_worked = true;
  if (code >= 0) {
    s_net_watch.ok();  // an answer, even an HTTP error: the connection works
    plat::netRestartsClear();
  } else {
    s_net_watch.failed(now);
  }
  netWatchAct(now);
}

/** Wi-Fi not connected (network task, every pass while it lasts). */
void watchOffline() {
  const uint32_t now = plat::millis();
  s_net_watch.offline(now, s_wifi_worked);
  netWatchAct(now);
}

void netWatchAct(uint32_t now) {
  // A router without internet shouldn't restart the radar over and over:
  // after two restarts in a row that didn't help, only keep rejoining.
  const core::NetWatch::Action action = s_net_watch.poll(now, plat::netRestarts() < 2);
  if (action == core::NetWatch::Action::None) return;
  const unsigned secs = s_net_watch.failingForMs(now) / 1000;
  std::string text;
  if (action == core::NetWatch::Action::Reconnect) {
    std::printf("net: nothing answered for %u s - rejoining Wi-Fi\n", secs);
    text = "No internet for " + std::to_string(secs / 60) + " min - reconnecting Wi-Fi";
    plat::httpDropKept();
    plat::wifiReconnect();
  } else {
    std::printf("net: no internet for %u s - restarting (also resets the Wi-Fi chip)\n", secs);
    text = "No internet for " + std::to_string(secs / 60) + " min - restarting";
  }
  {
    std::lock_guard<std::mutex> lock(s_mu);
    s_notices.push_back(text);
  }
  if (action == core::NetWatch::Action::Restart) {
    plat::sleepMs(3000);  // long enough to read the notice
    plat::restartForNetwork();
  }
}

void networkLoop() {
  std::string body;
  std::vector<core::Aircraft> parsed;
  uint32_t last_fetch = 0;
  bool first = true;

  for (;;) {
    if (updating()) {
      // The update has the network (and the CPU) to itself: an install from
      // GitHub runs right here, in serviceUpdate(); one from a phone in the
      // web server's task, while this loop waits.
      if (plat::wifiStatus().state != plat::WifiState::Connected || !serviceUpdate()) plat::sleepMs(200);
      continue;
    }
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
    if (replayActive()) {
      // Replaying: no live traffic (the map still fills in).
      const bool online = plat::wifiStatus().state == plat::WifiState::Connected;
      if (!serviceReplay() && (menu || !maps::service(online))) plat::sleepMs(50);
      continue;
    }
    if (plat::wifiStatus().state != plat::WifiState::Connected) {
      watchOffline();  // the Wi-Fi driver is reconnecting; restart if that never works
      if (menu || !maps::service(false)) plat::sleepMs(100);
      continue;
    }
    if (serviceUpdate()) continue;  // asked for on the About page
    if (!first && now - last_fetch >= kMinFetchSpacingMs &&
        (serviceFavoriteLookup() || servicePlaceSearch() || (!menu && (serviceRoute() || serviceTrace())))) {
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
    watchNetwork(code);
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

DayLogTotals dayLogTotals(int days) {
  DayLogTotals out;
  std::vector<core::daylog::Day> all = dayLogHistory(days - 1);
  all.insert(all.begin(), dayLogToday());
  std::vector<uint32_t> unique;
  for (const core::daylog::Day& d : all) {
    if (d.date == 0) continue;
    ++out.days;
    out.sum += d.count();
    if (d.count() > out.best_count) {
      out.best_count = d.count();
      out.best_date = d.date;
    }
    std::vector<uint32_t> merged;
    std::set_union(unique.begin(), unique.end(), d.seen.begin(), d.seen.end(), std::back_inserter(merged));
    unique.swap(merged);
    for (int h = 0; h < 24; ++h) out.hourly[h] += d.hourly[h];
  }
  out.unique = static_cast<int>(unique.size());
  return out;
}

std::string dayLogCsv(int days) {
  std::vector<core::daylog::Day> all = dayLogHistory(days - 1);
  std::string out = core::daylog::csvHeader();
  for (auto it = all.rbegin(); it != all.rend(); ++it) out += core::daylog::csvLine(*it);  // oldest first
  const core::daylog::Day today = dayLogToday();
  if (today.date != 0) out += core::daylog::csvLine(today);
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
  reloadSounds();
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
  if (s_dirty && !s_menu_open.load() && now_ms - s_dirty_since >= kSaveDebounceMs) settingsFlush();

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

LogbookSummary logbookSummary(size_t top) {
  LogbookSummary out;
  std::lock_guard<std::mutex> lock(s_book_mu);
  out.loaded = s_book_loaded;
  out.aircraft = s_book.aircraft();
  out.visits = s_book.visits();
  out.start_date = s_book.startDate();
  out.learning = s_book.learning(today());
  out.frequent = s_book.frequent(top);
  out.newest = s_book.newest(top);
  return out;
}

std::string logbookPath() { return cardReady() ? sdPath(kLogbookFile) : ""; }

bool serviceReplay() { return serviceReplayImpl(); }

bool replayActive() {
  std::lock_guard<std::mutex> lock(s_replay_mu);
  return s_replay.active;
}

std::vector<int> replayDays() {
  std::vector<int> out;
  if (!cardReady()) return out;
  if (DIR* d = opendir(sdPath("replay").c_str())) {
    while (dirent* e = readdir(d)) {
      const int date = core::replay::dateOfFile(e->d_name);
      if (date != 0) out.push_back(date);
    }
    closedir(d);
  }
  std::sort(out.rbegin(), out.rend());
  return out;
}

void replayStart(int date) {
  {
    std::lock_guard<std::mutex> lock(s_replay_mu);
    s_replay = ReplayCtl();
    s_replay.active = true;
    s_replay.date = date;
    s_replay.tick_ms = plat::millis();
  }
  s_traffic.clear();
  std::printf("replay: start %d\n", date);
}

void replayStop() {
  {
    std::lock_guard<std::mutex> lock(s_replay_mu);
    if (!s_replay.active) return;
    s_replay = ReplayCtl();
  }
  s_traffic.clear();
  requestRefresh();
  std::printf("replay: back to live traffic\n");
}

ReplayState replayState() {
  std::lock_guard<std::mutex> lock(s_replay_mu);
  ReplayState st;
  st.active = s_replay.active;
  st.date = s_replay.date;
  st.ready = s_replay.loaded_date == s_replay.date && !s_replay.index.empty();
  st.loading = s_replay.active && s_replay.loaded_date != s_replay.date;
  st.pos_s = s_replay.pos_s;
  if (!s_replay.index.empty()) {
    st.first_s = s_replay.index.front().first;
    st.last_s = s_replay.index.back().first;
  }
  st.playing = s_replay.playing;
  st.speed = s_replay.speed;
  return st;
}

void replaySeek(int64_t t) {
  std::lock_guard<std::mutex> lock(s_replay_mu);
  s_replay.pos_s = t;
  s_replay.pos = static_cast<double>(t);
  s_replay.tick_ms = plat::millis();
}

void replayPlay(bool on) {
  std::lock_guard<std::mutex> lock(s_replay_mu);
  if (on && !s_replay.index.empty() && s_replay.pos_s >= s_replay.index.back().first) {
    s_replay.pos_s = s_replay.index.front().first;  // from the start again
    s_replay.pos = static_cast<double>(s_replay.pos_s);
  }
  s_replay.playing = on;
  s_replay.tick_ms = plat::millis();
}

void replaySpeed(int x) {
  std::lock_guard<std::mutex> lock(s_replay_mu);
  s_replay.speed = std::max(1, std::min(600, x));
}

bool serviceCardWork() {
  return serviceLogbook() || serviceScreenshot() || serviceRouteCache() || serviceLookupClear();
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

std::mutex s_search_mu;
PlaceSearch s_search;  // guarded by s_search_mu
bool s_search_pending = false;

void searchPlaces(const std::string& query) {
  std::lock_guard<std::mutex> lock(s_search_mu);
  s_search = PlaceSearch{};
  s_search.query = query;
  if (plat::wifiStatus().state != plat::WifiState::Connected) {
    s_search.state = PlaceSearchState::Failed;
    s_search.error = "Searching needs Wi-Fi";
    s_search_pending = false;
    return;
  }
  s_search.state = PlaceSearchState::Searching;
  s_search_pending = true;
}

PlaceSearch placeSearch() {
  std::lock_guard<std::mutex> lock(s_search_mu);
  return s_search;
}

bool servicePlaceSearch() {
  std::string query;
  {
    std::lock_guard<std::mutex> lock(s_search_mu);
    if (!s_search_pending) return false;
    s_search_pending = false;
    query = s_search.query;
  }
  std::string body;
  const std::string url = core::geocode::searchUrl(query);
  const int code = plat::httpGet(url.c_str(), &body, 10000);
  std::vector<core::geocode::Result> results;
  const bool ok = code == 200 && core::geocode::parse(body.data(), body.size(), &results);
  std::printf("place search: \"%s\" -> HTTP %d, %u found\n", query.c_str(), code, static_cast<unsigned>(results.size()));
  std::lock_guard<std::mutex> lock(s_search_mu);
  if (s_search.query != query || s_search_pending) return true;  // a newer search replaced this one
  if (ok) {
    s_search.state = PlaceSearchState::Done;
    s_search.results = std::move(results);
  } else {
    s_search.state = PlaceSearchState::Failed;
    s_search.error = code < 0 ? "No connection - try again" : code == 200 ? "Unexpected answer from the search service"
                                                                         : "Search service answered HTTP " + std::to_string(code);
  }
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

std::string backupText() {
  core::backup::Parts parts;
  parts.settings = core::settings_text::serialize(s_settings);
  parts.favorites = core::settings_text::serializeFavorites(favorites(), unresolvedFavorites());
  parts.silenced = core::settings_text::serializeFavorites(silenced());
  std::string title = plat::firmwareVersion();
  if (plat::timeValid()) {
    const std::tm t = core::localTime(plat::unixTimeMs() / 1000);
    char date[24];
    std::strftime(date, sizeof(date), "%Y-%m-%d %H:%M", &t);
    title += std::string(", ") + date;
  }
  return core::backup::make(title, parts);
}

bool restoreBackup(const std::string& text, std::string* summary) {
  core::backup::Parts parts;
  if (!core::backup::split(text, &parts)) {
    *summary = "That isn't a Plane Radar backup file";
    return false;
  }
  std::string done;
  if (parts.has_settings) {
    std::vector<std::string> problems;
    const int n = applySettingsText(parts.settings, &problems);
    done += std::to_string(n) + " settings";
    if (!problems.empty()) done += " (" + std::to_string(problems.size()) + " lines skipped)";
    settingsFlush();
  }
  std::vector<core::Favorite> quiet;
  if (parts.has_silenced) {
    std::vector<std::string> tails;  // silenced aircraft are saved by address only
    core::settings_text::parseFavorites(parts.silenced, &quiet, &tails);
    {
      std::lock_guard<std::mutex> lock(s_favorites_mu);
      s_silenced = quiet;
    }
    plat::silencedSave(quiet);
    done += (done.empty() ? "" : ", ") + std::to_string(quiet.size()) + " silenced";
  }
  if (parts.has_favorites) {
    std::vector<core::Favorite> favs;
    std::vector<std::string> tails;
    core::settings_text::parseFavorites(parts.favorites, &favs, &tails);
    favs.erase(std::remove_if(favs.begin(), favs.end(),
                              [&](const core::Favorite& f) { return core::containsHex(quiet, f.hex); }),
               favs.end());
    setFavorites(favs);
    for (const std::string& t : tails) lookupFavorite(t);
    done += (done.empty() ? "" : ", ") + std::to_string(favs.size() + tails.size()) + " favorites";
  }
  std::printf("backup: restored %s\n", done.c_str());
  *summary = "Restored " + done;
  return true;
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

void startNetworkTask() {
  plat::startTask("adsb", networkLoop, 16 * 1024, 4);
  // Card writes run here, below the feed and the map, so a slow card never holds them up.
  plat::startTask("card", [] {
    for (;;) {
      if (!s_card.runOne()) s_card.waitForJob(500);
    }
  }, 8 * 1024, 2);
}

void reloadSounds() { s_card.push(loadSoundsNow, 0, false); }

std::vector<std::string> customSounds() {
  std::lock_guard<std::mutex> lock(s_sounds_mu);
  return s_sounds_status;
}

void queueCardJob(std::function<void()> job, size_t bytes, bool droppable) {
  s_card.push(std::move(job), bytes, droppable);
}

bool serviceCardQueue() { return s_card.runOne(); }

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
