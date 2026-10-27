#pragma once

#include <cstdint>
#include <ctime>

namespace core {

/** One alarm. days: bit 0 = Sunday ... bit 6 = Saturday; 0 = once (then it switches off). */
struct Alarm {
  bool enabled = false;
  uint8_t hour = 7;
  uint8_t minute = 0;
  uint8_t days = 0x3E;  // Monday-Friday
};

constexpr int kMaxAlarms = 4;
constexpr int kSnoozeMinutes = 9;
constexpr int kAlarmRingMinutes = 10;  // gives up after this long

struct TimeZone {
  const char* name;
  const char* posix;  // POSIX TZ rule, including daylight saving
};

int timeZoneCount();
const TimeZone& timeZone(int index);  // out-of-range indexes give UTC

/** Selects the zone used by localTime(). Not thread-safe: call from one task. */
void applyTimeZone(int index);
/** Local time in the applied zone. */
std::tm localTime(int64_t unix_s);

/** True if the alarm should ring at this local minute. */
bool alarmMatches(const Alarm& a, const std::tm& t);

/** Hand angles in degrees clockwise from 12, from local time plus milliseconds. */
struct HandAngles {
  float hour, minute, second;
};
HandAngles handAngles(const std::tm& t, int millis, bool smooth_seconds);

/** "Mon-Fri", "Every day", "Weekends", "Once", or e.g. "Mon Wed Fri". */
void formatDays(uint8_t days, char* out, int n);
/** "7:05 AM" or "07:05". */
void formatTime(int hour, int minute, bool h24, char* out, int n);

}  // namespace core
