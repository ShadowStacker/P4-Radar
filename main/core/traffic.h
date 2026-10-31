#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

#include <string>

#include "core/favorites.h"
#include "core/geo.h"

namespace core {

/** One aircraft as reported by a readsb-style JSON feed (adsb.fi & co). */
struct Aircraft {
  char hex[8] = "";       // ICAO 24-bit address, lowercase hex (may start with '~')
  char callsign[9] = "";
  char reg[12] = "";
  char type[5] = "";      // ICAO type designator, e.g. A320
  char squawk[5] = "";
  char category[3] = "";  // emitter category, e.g. A3
  double lat = 0;
  double lon = 0;
  float track_deg = 0;    // direction of travel
  float heading_deg = 0;  // nose direction (falls back to track)
  float gs_kt = 0;
  int32_t alt_ft = 0;
  int32_t vrate_fpm = 0;
  float seen_pos_s = 0;   // age of the position at fetch time
  bool alt_valid = false;
  bool on_ground = false;
  bool emergency = false;
  bool military = false;  // readsb "dbFlags" bit 0, from the feed's aircraft database
};

struct TrailPoint {
  float lat;
  float lon;
  int32_t alt_ft;
};

constexpr int kTrailLen = 48;

/** An aircraft plus everything we remember about it across fetches. */
struct Track {
  Aircraft ac;
  uint32_t fix_ms = 0;         // local ms timestamp at which ac.lat/lon was true
  uint32_t first_seen_ms = 0;
  uint32_t last_seen_ms = 0;
  TrailPoint trail[kTrailLen];
  uint8_t trail_count = 0;
  uint8_t trail_head = 0;      // index of the next write
  bool close_alerted = false;
  bool favorite_near_alerted = false;
  bool watch_alerted = false;  // announced as a watchlist aircraft

  /** i = 0 is the oldest retained point. */
  const TrailPoint& trailAt(int i) const {
    return trail[(trail_head + kTrailLen - trail_count + i) % kTrailLen];
  }
};

/** Dead-reckoned position at now_ms (straight line along track at ground speed). */
LatLon positionAt(const Track& t, uint32_t now_ms);

struct AlertZone {
  LatLon center;
  double radius_km = 0;
  int32_t max_alt_ft = 0;
};

/** Favorites to watch for, and the radius (around home) for "nearby". */
struct FavoriteWatch {
  const std::vector<Favorite>* favorites = nullptr;
  /** Silenced aircraft: no new-aircraft, proximity or watchlist events (emergencies still count). */
  const std::vector<Favorite>* silenced = nullptr;
  LatLon center;
  double near_radius_km = 0;
  /** Watchlist: why an aircraft is on it ("" = it isn't). Announced once, inside watch_radius_km. */
  std::function<std::string(const Aircraft&)> watch;
  double watch_radius_km = 0;
};

struct WatchHit {
  std::string name;    // display name
  std::string reason;  // "military", "C17", ...
  std::string type;
  double km = 0;       // from home
};

struct UpdateEvents {
  int new_aircraft = 0;
  int new_emergencies = 0;
  int new_close_approaches = 0;
  std::vector<std::string> favorites_seen;  // display names, newly in range
  std::vector<std::string> favorites_near;  // display names, newly within near_radius_km
  std::vector<WatchHit> watch_hits;         // watchlist aircraft, newly within watch_radius_km
};

/**
 * Thread-safe store shared by the network task (writer) and the UI (reader).
 * Readers copy a snapshot only when version() changed.
 */
class TrafficStore {
 public:
  static constexpr uint32_t kDropAfterMs = 45000;
  static constexpr size_t kMaxTracks = 400;

  UpdateEvents update(const std::vector<Aircraft>& fresh, uint32_t now_ms, const AlertZone& zone,
                      const FavoriteWatch& watch = {});
  /** Copies all tracks. Returns the version the copy corresponds to. */
  uint32_t snapshot(std::vector<Track>& out) const;
  uint32_t version() const;
  uint32_t lastUpdateMs() const;
  void clear();

 private:
  mutable std::mutex mu_;
  std::vector<Track> tracks_;
  uint32_t version_ = 0;
  uint32_t last_update_ms_ = 0;
};

}  // namespace core
