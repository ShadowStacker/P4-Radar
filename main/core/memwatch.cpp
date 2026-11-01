#include "core/memwatch.h"

namespace core::memwatch {

void Watch::add(const Sample& s) {
  const char* why = "";
  if (s.ram_free < kRamFreeLow) why = "RAM";
  else if (s.ram_largest < kRamLargestLow) why = "RAM blocks";
  else if (s.psram_largest < kPsramLargestLow) why = "PSRAM blocks";
  if (why[0] != '\0') {
    ++low_run_;
    reason_ = why;
  } else {
    low_run_ = 0;
  }
}

bool restartAllowed(const Moment& m) {
  if (!m.time_valid || m.updating || m.alarm_active || m.idle_ms < kIdleMs) return false;
  const int minute = m.local.tm_hour * 60 + m.local.tm_min;
  if (!inTimeWindow(minute, kWindowFrom, kWindowTo)) return false;
  for (int k = 0; k <= kAlarmMarginMin; ++k) {
    std::tm t = m.local;
    t.tm_min += k;
    t.tm_sec = 0;
    t.tm_isdst = -1;
    // Normalize (minutes past the hour roll into the hour, and the weekday on).
    const int total = t.tm_hour * 60 + t.tm_min;
    t.tm_hour = (total / 60) % 24;
    t.tm_min = total % 60;
    if (total >= 24 * 60) t.tm_wday = (t.tm_wday + 1) % 7;
    for (int i = 0; i < m.alarm_count; ++i) {
      if (m.alarms[i].enabled && alarmMatches(m.alarms[i], t)) return false;
    }
  }
  return true;
}

}  // namespace core::memwatch
