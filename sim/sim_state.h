#pragma once

#include <cstdint>
#include <functional>
#include <string>

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

}  // namespace sim
