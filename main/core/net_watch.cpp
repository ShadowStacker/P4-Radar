#include "core/net_watch.h"

namespace core {

void NetWatch::ok() {
  failures_ = 0;
  offline_ = false;
  reconnected_ = false;
  restart_done_ = false;
}

void NetWatch::failed(uint32_t now_ms) {
  if (failures_ == 0) since_ms_ = now_ms;
  ++failures_;
  offline_ = false;
}

void NetWatch::offline(uint32_t now_ms, bool worked_before) {
  if (!worked_before) {
    ok();
    return;
  }
  if (failures_ == 0) since_ms_ = now_ms;
  failures_ = kMinFailures;  // straight to "failing": no answers are coming
  offline_ = true;
}

NetWatch::Action NetWatch::poll(uint32_t now_ms, bool restart_allowed) {
  if (!failing()) return Action::None;
  const uint32_t down = now_ms - since_ms_;
  if (down >= kRestartAfterMs && restart_allowed && !restart_done_) {
    restart_done_ = true;
    return Action::Restart;
  }
  if (!offline_ && down >= kReconnectAfterMs && (!reconnected_ || now_ms - last_reconnect_ms_ >= kReconnectEveryMs)) {
    reconnected_ = true;
    last_reconnect_ms_ = now_ms;
    return Action::Reconnect;
  }
  return Action::None;
}

}  // namespace core
