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

#include "core/settings.h"
#include "core/sound_synth.h"

namespace plat {

uint32_t millis();

// --- Settings persistence ----------------------------------------------------
/** Loads settings; leaves defaults in place for anything not stored. */
void settingsLoad(core::Settings* s);
void settingsSave(const core::Settings& s);
/** Erases all stored settings including Wi-Fi. */
void settingsErase();

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
 * appends the body to `body`. Must NOT be called from the LVGL task.
 */
int httpGet(const char* url, std::string* body, int timeout_ms);

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
/** Free heap / PSRAM in bytes for the About page. */
size_t freeInternalHeap();
size_t freePsram();

}  // namespace plat
