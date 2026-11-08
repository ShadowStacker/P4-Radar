#include "core/failover.h"

namespace core {

void SourceFailover::setPreferred(DataSource p, bool fallback) {
  fallback_ = fallback;
  if (initialized_ && p == preferred_) return;
  initialized_ = true;
  preferred_ = active_ = p;
  failures_ = switches_since_ok_ = 0;
  probing_ = false;
}

DataSource SourceFailover::next(uint32_t now_ms) {
  probing_ = active_ != preferred_ && static_cast<int32_t>(now_ms - retry_at_ms_) >= 0;
  return probing_ ? preferred_ : active_;
}

SourceFailover::Change SourceFailover::report(bool ok, uint32_t now_ms) {
  if (probing_) {
    probing_ = false;
    if (ok) {
      active_ = preferred_;
      failures_ = switches_since_ok_ = 0;
      return Change::BackToPreferred;
    }
    retry_at_ms_ = now_ms + kRetryPreferredMs;
    return Change::None;
  }
  if (ok) {
    failures_ = switches_since_ok_ = 0;
    return Change::None;
  }
  if (++failures_ < kFailuresToSwitch) return Change::None;
  failures_ = 0;
  if (active_ == DataSource::Local && !fallback_) return Change::None;  // your receiver or nothing
  // From your receiver to the first online feed; between online feeds in turn.
  active_ = active_ == DataSource::Local ? DataSource::AdsbFi
                                         : static_cast<DataSource>((static_cast<int>(active_) + 1) % kSourceCount);
  retry_at_ms_ = now_ms + (preferred_ == DataSource::Local ? kRetryLocalMs : kRetryPreferredMs);
  // Without internet every source fails: say so for one round, then go quiet.
  return ++switches_since_ok_ < kSourceCount ? Change::Switched : Change::None;
}

}  // namespace core
