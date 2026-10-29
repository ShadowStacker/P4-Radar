#pragma once

#include <cstdint>

namespace core::sun {

/** Sunrise and sunset on one day at one place. */
struct Times {
  enum class Kind : uint8_t { Normal, AlwaysUp, AlwaysDown };  // polar day / polar night
  Kind kind = Kind::Normal;
  int64_t rise_s = 0;  // unix time (UTC), Normal only
  int64_t set_s = 0;
};

/**
 * Sunrise and sunset (upper limb at the horizon, with refraction) for the
 * calendar date year-month-day at lat/lon (east positive). Accurate to about
 * a minute away from the polar circles (NOAA's simplified sunrise equation).
 */
Times times(int year, int month, int day, double lat, double lon);

/** Days since 1970-01-01 for a calendar date. */
int64_t daysFromCivil(int year, int month, int day);

}  // namespace core::sun
