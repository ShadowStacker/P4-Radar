#include "core/net_watch.h"

namespace core {

void NetWatch::ok() {
  failures_ = 0;
  reconnected_ = false;
  restart_done_ = false;
}

void NetWatch::failed(uint32_t now_ms) {
  if (failures_ == 0) since_ms_ = now_ms;
  ++failures_;
}

void NetWatch::offline() { ok(); }

NetWatch::Action NetWatch::poll(uint32_t now_ms, bool restart_allowed) {
  if (!failing()) return Action::None;
  const uint32_t down = now_ms - since_ms_;
  if (down >= kRestartAfterMs && restart_allowed && !restart_done_) {
    restart_done_ = true;
    return Action::Restart;
  }
  if (down >= kReconnectAfterMs && (!reconnected_ || now_ms - last_reconnect_ms_ >= kReconnectEveryMs)) {
    reconnected_ = true;
    last_reconnect_ms_ = now_ms;
    return Action::Reconnect;
  }
  return Action::None;
}

}  // namespace core
