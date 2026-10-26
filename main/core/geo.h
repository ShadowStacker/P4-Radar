#pragma once

#include <cmath>

namespace core {

constexpr double kPi = 3.14159265358979323846;
constexpr double kEarthRadiusKm = 6371.0088;
constexpr double kKmPerNm = 1.852;
constexpr double kKmPerMi = 1.609344;
constexpr double kKmPerDegLat = kPi / 180.0 * kEarthRadiusKm;  // ~111.2 km

struct LatLon {
  double lat = 0.0;
  double lon = 0.0;
};

inline double degToRad(double d) { return d * kPi / 180.0; }
inline double radToDeg(double r) { return r * 180.0 / kPi; }

/** Wraps a longitude into [-180, 180). */
inline double wrapLon(double lon) {
  lon = std::fmod(lon + 180.0, 360.0);
  if (lon < 0) lon += 360.0;
  return lon - 180.0;
}

/** Normalizes a bearing into [0, 360). */
inline double wrapDeg360(double deg) {
  deg = std::fmod(deg, 360.0);
  return deg < 0 ? deg + 360.0 : deg;
}

/** Great-circle distance in km (haversine). */
inline double distanceKm(const LatLon& a, const LatLon& b) {
  const double p1 = degToRad(a.lat), p2 = degToRad(b.lat);
  const double dp = p2 - p1;
  const double dl = degToRad(b.lon - a.lon);
  const double h = std::sin(dp / 2) * std::sin(dp / 2) +
                   std::cos(p1) * std::cos(p2) * std::sin(dl / 2) * std::sin(dl / 2);
  return 2.0 * kEarthRadiusKm * std::asin(std::sqrt(h < 1.0 ? h : 1.0));
}

/** Initial great-circle bearing from a to b, degrees [0, 360). */
inline double bearingDeg(const LatLon& a, const LatLon& b) {
  const double p1 = degToRad(a.lat), p2 = degToRad(b.lat);
  const double dl = degToRad(b.lon - a.lon);
  const double y = std::sin(dl) * std::cos(p2);
  const double x = std::cos(p1) * std::sin(p2) - std::sin(p1) * std::cos(p2) * std::cos(dl);
  return wrapDeg360(radToDeg(std::atan2(y, x)));
}

/** Point reached by travelling dist_km from `from` on initial bearing brg_deg. */
inline LatLon destination(const LatLon& from, double brg_deg, double dist_km) {
  const double d = dist_km / kEarthRadiusKm;
  const double b = degToRad(brg_deg);
  const double p1 = degToRad(from.lat), l1 = degToRad(from.lon);
  const double p2 = std::asin(std::sin(p1) * std::cos(d) + std::cos(p1) * std::sin(d) * std::cos(b));
  const double l2 = l1 + std::atan2(std::sin(b) * std::sin(d) * std::cos(p1),
                                    std::cos(d) - std::sin(p1) * std::sin(p2));
  return {radToDeg(p2), wrapLon(radToDeg(l2))};
}

/**
 * Local flat-earth (equirectangular) projection around a reference point.
 * Accurate to well under a pixel for the <= 500 km radii this radar shows.
 * x grows east, y grows north, both in km.
 */
struct LocalProjection {
  LatLon ref;
  double cos_lat = 1.0;

  explicit LocalProjection(const LatLon& r = {}) { setRef(r); }

  void setRef(const LatLon& r) {
    ref = r;
    cos_lat = std::cos(degToRad(r.lat));
    if (cos_lat < 0.01) cos_lat = 0.01;  // keep the poles finite
  }

  void toKm(const LatLon& p, double* x_km, double* y_km) const {
    *x_km = wrapLon(p.lon - ref.lon) * kKmPerDegLat * cos_lat;
    *y_km = (p.lat - ref.lat) * kKmPerDegLat;
  }

  LatLon fromKm(double x_km, double y_km) const {
    LatLon p;
    p.lat = ref.lat + y_km / kKmPerDegLat;
    if (p.lat > 89.9) p.lat = 89.9;
    if (p.lat < -89.9) p.lat = -89.9;
    p.lon = wrapLon(ref.lon + x_km / (kKmPerDegLat * cos_lat));
    return p;
  }
};

}  // namespace core
