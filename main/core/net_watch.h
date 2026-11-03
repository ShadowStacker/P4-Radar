#pragma once

#include <cstdint>

namespace core {

/**
 * Notices when the internet has stopped answering while Wi-Fi says it is
 * connected, and says what to try: reconnect Wi-Fi after a minute, then
 * restart the radar (which also resets the Wi-Fi chip). Only connection
 * failures count; any HTTP answer, even an error, shows the link works.
 */
class NetWatch {
 public:
  enum class Action { None, Reconnect, Restart };

  static constexpr uint32_t kReconnectAfterMs = 60 * 1000;
  static constexpr uint32_t kReconnectEveryMs = 2 * 60 * 1000;
  static constexpr uint32_t kRestartAfterMs = 5 * 60 * 1000;
  static constexpr int kMinFailures = 3;  // a single slow request is not an outage

  /** A request got an HTTP answer (any status). */
  void ok();
  /** A request failed without an answer (timeout, no route, TLS, DNS). */
  void failed(uint32_t now_ms);
  /** Wi-Fi is not connected: the Wi-Fi driver is already retrying, nothing to do here. */
  void offline();

  /**
   * What to do now (each step is returned once). `restart_allowed` is false
   * after recent restarts that didn't help, so a router without internet
   * doesn't make the radar restart over and over.
   */
  Action poll(uint32_t now_ms, bool restart_allowed);

  bool failing() const { return failures_ >= kMinFailures; }
  uint32_t failingForMs(uint32_t now_ms) const { return failing() ? now_ms - since_ms_ : 0; }

 private:
  int failures_ = 0;
  uint32_t since_ms_ = 0;
  uint32_t last_reconnect_ms_ = 0;
  bool reconnected_ = false;
  bool restart_done_ = false;
};

}  // namespace core
