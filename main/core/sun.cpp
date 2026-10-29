#include "core/sun.h"

#include <cmath>

#include "core/geo.h"

namespace core::sun {

int64_t daysFromCivil(int y, int m, int d) {
  // Howard Hinnant's days_from_civil.
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const int64_t yoe = y - era * 400;
  const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

Times times(int year, int month, int day, double lat, double lon) {
  // https://en.wikipedia.org/wiki/Sunrise_equation
  const double n = static_cast<double>(daysFromCivil(year, month, day)) + 2440588.0 - 2451545.0 + 0.0008;
  const double j_star = n - lon / 360.0;
  const double m = std::fmod(357.5291 + 0.98560028 * j_star, 360.0);
  const double mr = degToRad(m);
  const double c = 1.9148 * std::sin(mr) + 0.0200 * std::sin(2 * mr) + 0.0003 * std::sin(3 * mr);
  const double l = degToRad(std::fmod(m + c + 180.0 + 102.9372, 360.0));
  const double transit = 2451545.0 + j_star + 0.0053 * std::sin(mr) - 0.0069 * std::sin(2 * l);
  const double sin_d = std::sin(l) * std::sin(degToRad(23.4397));
  const double cos_d = std::cos(std::asin(sin_d));
  const double phi = degToRad(lat);
  const double cos_w = (std::sin(degToRad(-0.833)) - std::sin(phi) * sin_d) / (std::cos(phi) * cos_d);
  Times t;
  if (cos_w > 1.0) {
    t.kind = Times::Kind::AlwaysDown;
    return t;
  }
  if (cos_w < -1.0) {
    t.kind = Times::Kind::AlwaysUp;
    return t;
  }
  const double w = radToDeg(std::acos(cos_w)) / 360.0;
  auto to_unix = [](double jd) { return static_cast<int64_t>(std::llround((jd - 2440587.5) * 86400.0)); };
  t.rise_s = to_unix(transit - w);
  t.set_s = to_unix(transit + w);
  return t;
}

}  // namespace core::sun
