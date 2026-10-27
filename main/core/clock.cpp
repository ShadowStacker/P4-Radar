#include "core/clock.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace core {

namespace {

// Daylight saving rules as of 2026. North America: 2nd Sunday of March to
// 1st Sunday of November; EU: last Sunday of March to last Sunday of October.
const TimeZone kZones[] = {
    {"UTC", "UTC0"},
    {"Pacific (Los Angeles, Portland)", "PST8PDT,M3.2.0,M11.1.0"},
    {"Mountain (Denver, Boise)", "MST7MDT,M3.2.0,M11.1.0"},
    {"Arizona (Phoenix)", "MST7"},
    {"Central (Chicago, Dallas)", "CST6CDT,M3.2.0,M11.1.0"},
    {"Eastern (New York, Miami)", "EST5EDT,M3.2.0,M11.1.0"},
    {"Alaska (Anchorage)", "AKST9AKDT,M3.2.0,M11.1.0"},
    {"Hawaii (Honolulu)", "HST10"},
    {"Atlantic (Halifax)", "AST4ADT,M3.2.0,M11.1.0"},
    {"Newfoundland (St. John's)", "NST3:30NDT,M3.2.0,M11.1.0"},
    {"Mexico City", "CST6"},
    {"Sao Paulo", "<-03>3"},
    {"London, Dublin, Lisbon", "GMT0BST,M3.5.0/1,M10.5.0"},
    {"Central Europe (Amsterdam, Paris, Berlin)", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Eastern Europe (Athens, Helsinki)", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"Istanbul, Moscow", "<+03>-3"},
    {"Dubai", "<+04>-4"},
    {"India (Delhi, Mumbai)", "IST-5:30"},
    {"Bangkok, Jakarta", "<+07>-7"},
    {"China, Singapore, Perth", "CST-8"},
    {"Japan, Korea", "JST-9"},
    {"Brisbane", "AEST-10"},
    {"Sydney, Melbourne", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
    {"Auckland", "NZST-12NZDT,M9.5.0,M4.1.0/3"},
};
constexpr int kZoneCount = sizeof(kZones) / sizeof(kZones[0]);

}  // namespace

int timeZoneCount() { return kZoneCount; }

const TimeZone& timeZone(int index) { return kZones[index >= 0 && index < kZoneCount ? index : 0]; }

void applyTimeZone(int index) {
  setenv("TZ", timeZone(index).posix, 1);
  tzset();
}

std::tm localTime(int64_t unix_s) {
  const std::time_t t = static_cast<std::time_t>(unix_s);
  std::tm out{};
  localtime_r(&t, &out);
  return out;
}

bool alarmMatches(const Alarm& a, const std::tm& t) {
  if (!a.enabled || t.tm_hour != a.hour || t.tm_min != a.minute) return false;
  return a.days == 0 || (a.days & (1u << t.tm_wday)) != 0;
}

HandAngles handAngles(const std::tm& t, int millis, bool smooth_seconds) {
  const float sec = t.tm_sec + (smooth_seconds ? millis / 1000.0f : 0.0f);
  const float min = t.tm_min + sec / 60.0f;
  const float hr = (t.tm_hour % 12) + min / 60.0f;
  return {hr * 30.0f, min * 6.0f, sec * 6.0f};
}

void formatDays(uint8_t days, char* out, int n) {
  days &= 0x7F;
  if (days == 0) {
    std::snprintf(out, n, "Once");
  } else if (days == 0x7F) {
    std::snprintf(out, n, "Every day");
  } else if (days == 0x3E) {
    std::snprintf(out, n, "Mon-Fri");
  } else if (days == 0x41) {
    std::snprintf(out, n, "Weekends");
  } else {
    static const char* const kNames[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    out[0] = '\0';
    int len = 0;
    // Monday first, like most calendars.
    for (int k = 0; k < 7; ++k) {
      const int d = (k + 1) % 7;
      if (!(days & (1u << d))) continue;
      len += std::snprintf(out + len, len < n ? n - len : 0, "%s%s", len ? " " : "", kNames[d]);
    }
  }
}

void formatTime(int hour, int minute, bool h24, char* out, int n) {
  if (h24) {
    std::snprintf(out, n, "%02d:%02d", hour, minute);
  } else {
    const int h12 = hour % 12 == 0 ? 12 : hour % 12;
    std::snprintf(out, n, "%d:%02d %s", h12, minute, hour < 12 ? "AM" : "PM");
  }
}

}  // namespace core
