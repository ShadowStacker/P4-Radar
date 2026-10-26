#pragma once

#include <cstdint>
#include <string>

#include "core/geo.h"
#include "core/settings.h"
#include "core/sound_synth.h"
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
  int last_count = 0;
  char error[48] = "";
};
FeedStatus feedStatus();

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
