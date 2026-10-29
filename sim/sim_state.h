#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "platform/platform.h"

namespace sim {

/** Virtual clock (ms) driving LVGL ticks and plat::millis(). */
extern uint32_t g_ms;

/** Scripted touch state (headless) or mouse state (SDL). */
struct Touch {
  bool down = false;
  int count = 0;
  int16_t x[2] = {0, 0};
  int16_t y[2] = {0, 0};
};
extern Touch g_touch;

/** When false, the simulator starts without Wi-Fi credentials (first boot). */
extern bool g_preset_wifi;
/** Directory for screenshots and rendered sounds. */
extern std::string g_out_dir;

/** Answers trace requests (hex -> uncompressed trace JSON, "" = 404). */
extern std::function<std::string(const std::string&)> g_trace_server;

/** Answers adsbdb aircraft lookups (hex -> JSON, "" = 404). */
extern std::function<std::string(const std::string&)> g_route_server;

/** The fake GitHub reports a newer release (v1.13.0) instead of the running one. */
extern bool g_update_available;

/** The phone page's request handler, once the app started the web server. */
extern plat::WebHandler g_web_handler;
/** Bytes of firmware "written" by the last upload, and whether it was committed. */
extern size_t g_firmware_written;
extern bool g_firmware_committed;

}  // namespace sim
