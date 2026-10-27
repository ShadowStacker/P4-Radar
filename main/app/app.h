#pragma once

#include <cstdint>
#include <string>

#include <vector>

#include "core/favorites.h"
#include "core/geo.h"
#include "core/settings.h"
#include "core/sound_synth.h"
#include "core/trace.h"
#include "core/traffic.h"

/**
 * Platform-independent application state: settings, the shared traffic
 * store and the ADS-B polling loop. UI code reads from here; the network task
 * writes into the traffic store.
 */
namespace app {

/** Loads settings and applies brightness/volume. Call once at boot. */
void init();

/** Live settings. Only mutate from the LVGL task, then call settingsChanged(). */
core::Settings& settings();
/** Persists settings (debounced) and re-applies brightness, volume, theme. */
void settingsChanged();
/** Writes pending settings to flash immediately. */
void settingsFlush();
/** Called periodically from the UI task (handles debounced save, dimming). */
void tick(uint32_t now_ms);
/** Reset the idle timer (any touch). Returns true if the screen was dimmed. */
bool userActivity();

core::TrafficStore& traffic();

/** Area the radar is looking at; the poller fetches around it. */
void setQueryArea(const core::LatLon& center, double radius_km);
void queryArea(core::LatLon* center, double* radius_km);
/** Ask the poller to fetch soon (e.g. after a large pan), rate-limited. */
void requestRefresh();

struct FeedStatus {
  bool ever_ok = false;
  bool last_ok = false;
  uint32_t last_ok_ms = 0;
  int last_http = 0;
  int last_count = 0;   // aircraft shown (after filters)
  int last_total = 0;   // aircraft in the feed's response
  char error[48] = "";
};
FeedStatus feedStatus();

// --- Favorites (thread-safe) ---------------------------------------------------
std::vector<core::Favorite> favorites();
bool isFavorite(const char* hex);
/** Adds or removes the aircraft. Returns true if it is now a favorite. */
bool toggleFavorite(const core::Aircraft& a);
void removeFavorite(const char* hex);
/** Next on-screen notice from the poller ("RCH471 detected"), if any. */
bool popNotice(std::string* out);

// --- Extended trail (flight history) of the selected aircraft ------------------
enum class TraceStatus : uint8_t { None, Loading, Ready, Unavailable };

struct TraceInfo {
  TraceStatus status = TraceStatus::None;
  char hex[8] = "";
  uint32_t version = 0;   // changes whenever the points change
  float duration_s = 0;   // time covered by the points
};

/**
 * Asks the poller to download the flight history of `hex` (refreshed every
 * couple of minutes while it stays requested). "" cancels. Does nothing when
 * the extended trail is switched off.
 */
void requestTrace(const char* hex);
TraceInfo traceInfo();
/** Copies the trace points; returns their version. */
uint32_t traceCopy(core::trace::Trace* out);
/**
 * Downloads a pending trace, if any. Called by the network task; returns
 * true if it made a request.
 */
bool serviceTrace();

/** Feeds one HTTP response through parse -> traffic store -> sounds. */
void ingest(int http_code, const std::string& body);

/** Starts the background ADS-B poller task. */
void startNetworkTask();

/** Plays a sound if the user enabled that category. */
void sound(core::Sound s);

/** Formats a distance in the user's units ("12.5 km", "8 mi", "6.7 nm"). */
void formatDistance(double km, char* out, int out_len, bool compact = false);
const char* unitSuffix();
double kmToUnits(double km);
double unitsToKm(double v);

}  // namespace app
