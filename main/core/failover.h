#pragma once

#include <cstdint>

#include "core/settings.h"

namespace core {

/**
 * Which ADS-B source to ask. After a few failures in a row the next source
 * is used; every ten minutes the chosen (preferred) one is tried again, and
 * used again as soon as it answers.
 */
class SourceFailover {
 public:
  static constexpr int kFailuresToSwitch = 3;
  static constexpr uint32_t kRetryPreferredMs = 10 * 60 * 1000;
  static constexpr int kSourceCount = 3;

  enum class Change : uint8_t {
    None,
    Switched,       // moved on to active() because the previous source kept failing
    BackToPreferred,
  };

  /** The source picked in settings; a new choice is used right away. */
  void setPreferred(DataSource p);
  /** Source for the next fetch. */
  DataSource next(uint32_t now_ms);
  /** Result of that fetch. Change::Switched is reported for one round of switching, then quietly. */
  Change report(bool ok, uint32_t now_ms);

  DataSource active() const { return active_; }
  DataSource preferred() const { return preferred_; }

 private:
  DataSource preferred_ = DataSource::AdsbFi;
  DataSource active_ = DataSource::AdsbFi;
  bool initialized_ = false;
  bool probing_ = false;  // this fetch goes to the preferred source to see if it's back
  int failures_ = 0;
  int switches_since_ok_ = 0;
  uint32_t retry_at_ms_ = 0;
};

}  // namespace core
