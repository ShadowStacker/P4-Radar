#pragma once

#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "core/geo.h"

namespace sim {

/** Deterministic synthetic air traffic that serializes to readsb JSON. */
class FakeTraffic {
 public:
  FakeTraffic(core::LatLon home, uint32_t seed);

  void advance(double dt_s);
  /** readsb/adsb.fi style response for aircraft within radius of center. */
  std::string json(const core::LatLon& center, double radius_km) const;
  void setEmergency(bool on);
  /**
   * readsb trace_full JSON for one aircraft: an earlier leg plus the current
   * flight, a climbing, turning path ending at its present position.
   * Empty if the aircraft is unknown.
   */
  std::string traceJson(const std::string& hex) const;
  /** adsbdb.com style /v0/aircraft/<hex>?callsign= response; "" if unknown. */
  std::string routeJson(const std::string& hex) const;
  /** Moves the military aircraft back near home (for screenshots). */
  void recallMilitary();

 private:
  struct Plane {
    std::string hex, flight, reg, type, category, squawk;
    core::LatLon pos;
    double track, gs, alt, vrate, turn;
    bool military = false;
  };
  void spawn(Plane& p, bool near_home);

  core::LatLon home_;
  std::mt19937 rng_;
  std::vector<Plane> planes_;
};

}  // namespace sim
