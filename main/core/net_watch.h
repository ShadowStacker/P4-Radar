#pragma once

#include <cstdint>

namespace core {

/**
 * Notices when the internet has stopped answering, and says what to try:
 * reconnect Wi-Fi after a minute, then restart the radar (which also resets
 * the Wi-Fi chip). Only connection failures count; any HTTP answer, even an
 * error, shows the link works. Wi-Fi that can't reconnect counts too, once
 * it has worked since the start: the Wi-Fi chip has been seen to get stuck
 * that way, failing to rejoin a network that is right there.
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
  /**
   * Wi-Fi is not connected. Before it has ever connected (wrong password,
   * out of range at start) that's left to the Wi-Fi driver; after, the
   * outage counts toward a restart. No "reconnect" step then: the driver is
   * already retrying, and going through the saved networks.
   */
  void offline(uint32_t now_ms, bool worked_before);

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
  bool offline_ = false;
};

}  // namespace core
