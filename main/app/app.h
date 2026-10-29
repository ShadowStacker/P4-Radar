#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "core/favorites.h"
#include "core/geo.h"
#include "core/route.h"
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
/** Counts settingsChanged() calls, to tell whether anything changed since a snapshot. */
uint32_t settingsGeneration();
/**
 * UI task: runs after settings were replaced from settings.txt on the SD
 * card (Wi-Fi, time zone and hardware are already applied), with the
 * settings from before, so the UI can re-theme or restart.
 */
void onSettingsLoaded(std::function<void(const core::Settings& before)> fn);
/** Factory reset: erases saved settings and favorites, and settings.txt on the card. */
void factoryReset();
/** Called periodically from the UI task (handles debounced save, dimming). */
void tick(uint32_t now_ms);
/**
 * Sleep: backlight off and sounds muted. The UI wakes it on the next tap.
 * Alarms ring and wake it only with settings().wake_on_alarm; otherwise they
 * pass silently while asleep.
 */
void setSleeping(bool on);
bool sleeping();
/** Reset the idle timer (any touch). Returns true if the screen was dimmed. */
bool userActivity();
/**
 * The settings menu is open: the radar isn't visible, so the poller pauses
 * map tiles and flight-history downloads to keep the menu responsive.
 */
void setMenuOpen(bool open);

// --- Wi-Fi networks (UI task) ------------------------------------------------------
/**
 * Saved networks, the one to use first at [0] (Settings::wifi_ssid), then
 * Settings::wifi_saved. Up to 1 + core::kMaxSavedWifi.
 */
std::vector<core::SavedWifi> savedWifi();
/** True if `ssid` is saved; fills its password. */
bool isSavedWifi(const char* ssid, std::string* pass = nullptr);
/**
 * Connects and makes it the network to use first. The previous one stays
 * saved (the oldest is dropped when all five slots are taken).
 */
void useWifi(const char* ssid, const char* pass);
/** Removes a saved network. Forgetting the current one moves on to the next saved one. */
void forgetWifi(const char* ssid);

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
/** Adds a favorite directly. False if it's already saved or the list is full. */
bool addFavorite(const core::Favorite& f);
/**
 * Looks up a registration (tail number) or ICAO hex online and saves that
 * aircraft as a favorite. The result arrives as a notice ("N123AB added").
 */
void lookupFavorite(const std::string& registration_or_hex);
/** Runs a pending lookup (network task); true if it made a request. */
bool serviceFavoriteLookup();
/** Replaces the whole list (used to undo edits). */
void setFavorites(const std::vector<core::Favorite>& favs);
/**
 * Network task: a card was just mounted. Reads settings.txt and
 * favorites.txt (or writes them if missing, or if this board has changes the
 * card hasn't seen).
 */
void sdFilesMounted();
/** Network task: writes the text files when they're out of date. True if it wrote one. */
bool serviceSdFiles();
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

// --- Airline / route of the selected aircraft (adsbdb.com) --------------------
enum class RouteStatus : uint8_t { None, Loading, Ready, Unknown };

struct RouteState {
  RouteStatus status = RouteStatus::None;
  char hex[8] = "";
  char callsign[9] = "";
  core::route::Info info;
};

/** Asks the poller to look up the aircraft ("" cancels). Cached per aircraft and callsign. */
void requestRoute(const char* hex, const char* callsign);
RouteState routeState();
/** Runs a pending lookup (network task); true if it made a request. */
bool serviceRoute();

/** Feeds one HTTP response through parse -> traffic store -> sounds. */
void ingest(int http_code, const std::string& body);

/** Starts the background ADS-B poller task. */
void startNetworkTask();

// --- Clock and alarms (UI task) ------------------------------------------------
/** Re-applies settings().time_zone to local-time conversions. */
void timeZoneChanged();
/** Index of the alarm that is ringing, or -1. Alarms ring even when muted. */
int alarmRinging();
void snoozeAlarm();  // rings again in core::kSnoozeMinutes
void stopAlarm();
/** Rings alarm `index` now (Clock settings test button). */
void testAlarm(int index);
/** Minutes until a snoozed alarm rings again, or -1. */
int snoozeMinutesLeft();

// --- Firmware updates from GitHub releases ---------------------------------------
enum class UpdateState : uint8_t {
  Idle,
  Checking,
  UpToDate,
  Available,  // `latest` can be installed
  NoImage,    // newer release, but without an update image for this board (flash it over USB)
  Failed,     // see `error`
  Installing,
  Installed,  // restart to run it
};

struct UpdateStatus {
  UpdateState state = UpdateState::Idle;
  bool install_failed = false;  // Failed came from an install, not a check
  char latest[24] = "";         // "v1.13.0"
  char error[72] = "";
  size_t done = 0, total = 0;   // bytes, while installing
};

/** Asks the poller to look up the latest release (it also checks by itself every hour). */
void checkForUpdate();
/** Downloads and installs the release found by the last check (state Available). */
void installUpdate();
UpdateStatus updateStatus();
/** Network task: runs a pending check or install. True if it did something. */
bool serviceUpdate();

/** Plays a sound if the user enabled that category. */
void sound(core::Sound s);

/** Formats a distance in the user's units ("12.5 km", "8 mi", "6.7 nm"). */
void formatDistance(double km, char* out, int out_len, bool compact = false);
const char* unitSuffix();
double kmToUnits(double km);
double unitsToKm(double v);

}  // namespace app
