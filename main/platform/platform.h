#pragma once

/**
 * Hardware/OS abstraction. The UI and core only talk to the device through
 * these functions, so the exact same UI code also builds in the desktop
 * simulator (sim/), where they are faked.
 *
 * Unless noted, functions are safe to call from the LVGL task.
 */

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "core/favorites.h"
#include "core/settings.h"
#include "core/sound_synth.h"

namespace plat {

uint32_t millis();
/** Microsecond counter for profiling (wraps). */
uint64_t micros();

// --- Settings persistence ----------------------------------------------------
/** Loads settings; leaves defaults in place for anything not stored. */
void settingsLoad(core::Settings* s);
void settingsSave(const core::Settings& s);
/** Erases all stored settings including Wi-Fi. */
void settingsErase();
/** Saved aircraft, stored separately from settings. */
void favoritesLoad(std::vector<core::Favorite>* out);
void favoritesSave(const std::vector<core::Favorite>& favs);

// --- Wi-Fi -------------------------------------------------------------------
enum class WifiState : uint8_t { Idle, Connecting, Connected, Failed };

struct WifiStatus {
  WifiState state = WifiState::Idle;
  char ssid[33] = "";
  char ip[16] = "";
  int rssi = 0;
  /** Short human-readable reason after a failure ("Wrong password", ...). */
  char reason[32] = "";
};

struct WifiNetwork {
  std::string ssid;
  int rssi = 0;
  bool secure = true;
};

WifiStatus wifiStatus();
/** Connects (and keeps reconnecting) to the network. Empty ssid disconnects. */
void wifiConnect(const char* ssid, const char* pass);
void wifiScanStart();
/** True once the last scan finished; fills `out` sorted by signal strength. */
bool wifiScanResults(std::vector<WifiNetwork>* out);

// --- HTTP ----------------------------------------------------------------------
/**
 * Blocking HTTPS GET; returns the HTTP status (or <0 on transport error) and
 * appends the body to `body` as received (a gzip-encoded body stays
 * compressed). `headers` is an optional nullptr-terminated list of
 * name/value pairs. Must NOT be called from the LVGL task.
 */
int httpGet(const char* url, std::string* body, int timeout_ms, const char* const* headers = nullptr);

/**
 * Like httpGet(), but keeps the connection open for the next call to the
 * same host (map tiles: one TLS handshake instead of one per tile). Only
 * call from one task.
 */
int httpGetKeepAlive(const char* url, std::string* body, int timeout_ms);

// --- SD card (call from the network task only) ---------------------------------
struct SdInfo {
  bool mounted = false;
  uint64_t total_bytes = 0;
  uint64_t free_bytes = 0;
  char error[48] = "";  // why mounting failed ("No card", "Not FAT formatted", ...)
};
/** Mounts the card (no-op if mounted); fills `info` either way. */
bool sdMount(SdInfo* info);
void sdUnmount();
/** Current state and free space. */
SdInfo sdInfo();
/** Directory the card is mounted at ("/sdcard"). */
const char* sdRoot();

// --- Time ----------------------------------------------------------------------
/** Wall-clock time in ms since 1970 (UTC). Meaningless until timeValid(). */
int64_t unixTimeMs();
/** True once the clock was set from the internet (SNTP). */
bool timeValid();

// --- Display -----------------------------------------------------------------
void setBrightness(int percent);
/** Raw touch points currently down (screen px), for multi-finger gestures. */
struct TouchPoint {
  int16_t x;
  int16_t y;
};
int touchPoints(TouchPoint* out, int max);

// --- Audio -------------------------------------------------------------------
/** Queues a sound; never blocks. Ignored when audio is unavailable. */
void playSound(core::Sound s);
void setVolume(int percent);

// --- Tasks / system ------------------------------------------------------------
void startTask(const char* name, std::function<void()> fn, size_t stack_bytes, int priority);
void sleepMs(uint32_t ms);
void restart();
const char* firmwareVersion();
/**
 * Large, long-lived buffers (images, canvases): PSRAM on the device, 64-byte
 * aligned. Internal RAM is reserved for the RTOS, Wi-Fi and display DMA.
 */
void* allocLarge(size_t bytes);
void freeLarge(void* p);

/** Free heap / PSRAM in bytes for the About page. */
size_t freeInternalHeap();
size_t freePsram();

/**
 * One line for the serial log: internal RAM (free, lowest ever, largest
 * block) and PSRAM free, plus CPU load per core and the busiest tasks since
 * the previous call. For diagnosing slowdowns.
 */
std::string healthReport();

}  // namespace plat
