#include "core/traffic.h"

#include <cstring>

namespace core {

namespace {

constexpr double kKmPerKtMs = kKmPerNm / 3600000.0;  // knots -> km per ms
constexpr uint32_t kMaxExtrapolateMs = 40000;
constexpr double kTrailMinStepKm = 0.12;

void pushTrail(Track& t, const Aircraft& a) {
  if (t.trail_count > 0) {
    const TrailPoint& last = t.trail[(t.trail_head + kTrailLen - 1) % kTrailLen];
    if (distanceKm({last.lat, last.lon}, {a.lat, a.lon}) < kTrailMinStepKm) return;
  }
  t.trail[t.trail_head] = {static_cast<float>(a.lat), static_cast<float>(a.lon), a.alt_ft};
  t.trail_head = (t.trail_head + 1) % kTrailLen;
  if (t.trail_count < kTrailLen) ++t.trail_count;
}

std::string displayName(const Aircraft& a) {
  if (a.callsign[0]) return a.callsign;
  if (a.reg[0]) return a.reg;
  return a.hex;
}

bool inZone(const Aircraft& a, const AlertZone& z) {
  if (z.radius_km <= 0 || a.on_ground) return false;
  if (a.alt_valid && a.alt_ft > z.max_alt_ft) return false;
  return distanceKm(z.center, {a.lat, a.lon}) <= z.radius_km;
}

}  // namespace

LatLon positionAt(const Track& t, uint32_t now_ms) {
  const LatLon fix{t.ac.lat, t.ac.lon};
  if (t.ac.on_ground || t.ac.gs_kt < 1.0f) return fix;
  uint32_t age = now_ms - t.fix_ms;
  if (static_cast<int32_t>(age) < 0) age = 0;
  if (age > kMaxExtrapolateMs) age = kMaxExtrapolateMs;
  return destination(fix, t.ac.track_deg, t.ac.gs_kt * kKmPerKtMs * age);
}

UpdateEvents TrafficStore::update(const std::vector<Aircraft>& fresh, uint32_t now_ms,
                                  const AlertZone& zone, const FavoriteWatch& watch) {
  UpdateEvents ev;
  std::lock_guard<std::mutex> lock(mu_);
  const bool first = (version_ == 0);

  for (const Aircraft& a : fresh) {
    const bool quiet = watch.silenced != nullptr && containsHex(*watch.silenced, a.hex);
    Track* t = nullptr;
    for (Track& existing : tracks_) {
      if (std::strcmp(existing.ac.hex, a.hex) == 0) {
        t = &existing;
        break;
      }
    }
    if (t == nullptr) {
      if (tracks_.size() >= kMaxTracks) continue;
      tracks_.emplace_back();
      t = &tracks_.back();
      t->first_seen_ms = now_ms;
      if (!first && !quiet) ++ev.new_aircraft;
      if (a.emergency) ++ev.new_emergencies;
      // Favorites are worth announcing even on the very first fetch.
      if (watch.favorites != nullptr && containsHex(*watch.favorites, a.hex)) {
        ev.favorites_seen.push_back(displayName(a));
      }
    } else if (a.emergency && !t->ac.emergency) {
      ++ev.new_emergencies;
    }

    t->ac = a;
    const uint32_t age_ms = static_cast<uint32_t>(a.seen_pos_s * 1000.0f);
    t->fix_ms = now_ms - age_ms;
    t->last_seen_ms = now_ms;
    pushTrail(*t, a);

    const bool close = inZone(a, zone);
    if (close && !t->close_alerted && !quiet) ++ev.new_close_approaches;
    t->close_alerted = close;

    if (watch.favorites != nullptr && watch.near_radius_km > 0 && containsHex(*watch.favorites, a.hex)) {
      const bool near = distanceKm(watch.center, {a.lat, a.lon}) <= watch.near_radius_km;
      if (near && !t->favorite_near_alerted) ev.favorites_near.push_back(displayName(a));
      t->favorite_near_alerted = near;
    }

    if (watch.watch && !t->watch_alerted && !quiet) {
      const double km = distanceKm(watch.center, {a.lat, a.lon});
      if (km <= watch.watch_radius_km) {
        const std::string why = watch.watch(a);
        if (!why.empty()) {
          t->watch_alerted = true;  // once per visit (until it's dropped)
          ev.watch_hits.push_back({displayName(a), why, a.type, km});
        }
      }
    }
  }

  // Forget aircraft that have not been reported for a while.
  size_t w = 0;
  for (size_t r = 0; r < tracks_.size(); ++r) {
    if (now_ms - tracks_[r].last_seen_ms <= kDropAfterMs) {
      if (w != r) tracks_[w] = tracks_[r];
      ++w;
    }
  }
  tracks_.resize(w);

  ++version_;
  last_update_ms_ = now_ms;
  return ev;
}

uint32_t TrafficStore::snapshot(std::vector<Track>& out) const {
  std::lock_guard<std::mutex> lock(mu_);
  out = tracks_;
  return version_;
}

uint32_t TrafficStore::version() const {
  std::lock_guard<std::mutex> lock(mu_);
  return version_;
}

uint32_t TrafficStore::lastUpdateMs() const {
  std::lock_guard<std::mutex> lock(mu_);
  return last_update_ms_;
}

void TrafficStore::clear() {
  std::lock_guard<std::mutex> lock(mu_);
  tracks_.clear();
  ++version_;
}

}  // namespace core
