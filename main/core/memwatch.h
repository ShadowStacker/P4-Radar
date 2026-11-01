#pragma once

#include <cstddef>
#include <cstdint>
#include <ctime>

#include "core/clock.h"

/**
 * Memory health watch. Free memory is sampled once a minute; when internal
 * RAM (or the largest free block of it, or of PSRAM) stays too low for half
 * an hour, the radar is due for a fresh start, which it takes quietly in the
 * night (see restartAllowed()).
 */
namespace core::memwatch {

struct Sample {
  size_t ram_free = 0;
  size_t ram_largest = 0;
  size_t psram_largest = 0;
};

// Lower than anything seen in normal running.
constexpr size_t kRamFreeLow = 12 * 1024;
constexpr size_t kRamLargestLow = 4 * 1024;
constexpr size_t kPsramLargestLow = 512 * 1024;
constexpr int kLowSamples = 30;  // minutes in a row

class Watch {
 public:
  /** One sample (a minute apart). */
  void add(const Sample& s);
  /** Low for kLowSamples samples in a row. */
  bool due() const { return low_run_ >= kLowSamples; }
  /** What was low last ("" if nothing): "RAM", "RAM blocks" or "PSRAM blocks". */
  const char* reason() const { return reason_; }
  int lowRun() const { return low_run_; }

 private:
  int low_run_ = 0;
  const char* reason_ = "";
};

/** What restartAllowed() looks at. */
struct Moment {
  std::tm local{};            // local time now
  bool time_valid = false;
  uint32_t idle_ms = 0;       // since the last touch (or phone page visit)
  bool alarm_active = false;  // ringing or snoozed
  bool updating = false;
  const Alarm* alarms = nullptr;
  int alarm_count = 0;
};

constexpr int kWindowFrom = 3 * 60;  // 3:00
constexpr int kWindowTo = 5 * 60;    // to 5:00
constexpr uint32_t kIdleMs = 15 * 60 * 1000;
constexpr int kAlarmMarginMin = 30;

/**
 * True when a fresh start would bother nobody: 3-5 AM, nobody has touched it
 * for 15 minutes, no alarm ringing or due in the next 30 minutes, no update
 * running.
 */
bool restartAllowed(const Moment& m);

}  // namespace core::memwatch
