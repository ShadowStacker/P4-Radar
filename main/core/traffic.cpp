#include "core/traffic.h"

#include <algorithm>
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

/** An address as a number for fast lookups ("~" non-ICAO ones get the top bit); 0 if it isn't one. */
uint32_t addressKey(const char* hex) {
  const bool non_icao = hex[0] == '~';
  const char* p = non_icao ? hex + 1 : hex;
  uint32_t v = 0;
  int n = 0;
  for (; p[n] != '\0'; ++n) {
    const char c = p[n];
    int d;
    if (c >= '0' && c <= '9') d = c - '0';
    else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
    else return 0;
    if (n >= 7) return 0;
    v = v * 16 + static_cast<uint32_t>(d);
  }
  if (n == 0) return 0;
  return (v | (non_icao ? 0x80000000u : 0)) + 1;  // never 0
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
                                  const AlertZone& zone, const FavoriteWatch& watch, const LatLon* focus) {
  UpdateEvents ev;
  std::lock_guard<std::mutex> lock(mu_);
  const bool first = (version_ == 0);

  // Forget aircraft that have not been reported for a while - first, so they
  // don't keep newcomers out when the store is full.
  size_t w = 0;
  for (size_t r = 0; r < tracks_.size(); ++r) {
    if (now_ms - tracks_[r].last_seen_ms <= kDropAfterMs) {
      if (w != r) tracks_[w] = tracks_[r];
      ++w;
    }
  }
  tracks_.resize(w);
  tracks_.reserve(kMaxTracks);  // no reallocation below: pointers into it stay valid

  // More than fit (a wide area over a busy region): the nearest ones.
  std::vector<Aircraft> nearest;
  const std::vector<Aircraft>* list = &fresh;
  if (fresh.size() > kMaxTracks && focus != nullptr) {
    std::vector<std::pair<double, size_t>> by_dist;
    by_dist.reserve(fresh.size());
    for (size_t i = 0; i < fresh.size(); ++i) by_dist.emplace_back(distanceKm(*focus, {fresh[i].lat, fresh[i].lon}), i);
    std::nth_element(by_dist.begin(), by_dist.begin() + kMaxTracks, by_dist.end());
    by_dist.resize(kMaxTracks);
    std::sort(by_dist.begin(), by_dist.end(), [](const auto& x, const auto& y) { return x.second < y.second; });
    nearest.reserve(kMaxTracks);
    for (const auto& d : by_dist) nearest.push_back(fresh[d.second]);
    list = &nearest;
  }

  // The aircraft already known, by address: a binary search each instead of
  // comparing every fresh aircraft with every track (160,000 compares at 400).
  std::vector<std::pair<uint32_t, uint32_t>> index;  // (address key, track index), sorted
  index.reserve(tracks_.size());
  for (size_t i = 0; i < tracks_.size(); ++i) {
    const uint32_t k = addressKey(tracks_[i].ac.hex);
    if (k != 0) index.emplace_back(k, static_cast<uint32_t>(i));
  }
  std::sort(index.begin(), index.end());
  const size_t known = tracks_.size();  // tracks added below are not in the index
  auto find = [&](const char* hex) -> Track* {
    const uint32_t k = addressKey(hex);
    if (k != 0) {
      auto it = std::lower_bound(index.begin(), index.end(), std::make_pair(k, 0u));
      if (it != index.end() && it->first == k) {
        Track* t = &tracks_[it->second];
        if (std::strcmp(t->ac.hex, hex) == 0) return t;  // (a replaced track has a new address)
      }
    }
    // Odd addresses, and aircraft added or swapped in during this update: the slow way.
    for (size_t i = (k != 0 ? known : 0); i < tracks_.size(); ++i) {
      if (std::strcmp(tracks_[i].ac.hex, hex) == 0) return &tracks_[i];
    }
    if (k != 0) {
      for (size_t i = 0; i < known; ++i) {
        if (tracks_[i].last_seen_ms == now_ms && std::strcmp(tracks_[i].ac.hex, hex) == 0) return &tracks_[i];
      }
    }
    return nullptr;
  };

  for (const Aircraft& a : *list) {
    const bool quiet = watch.silenced != nullptr && containsHex(*watch.silenced, a.hex);
    Track* t = find(a.hex);
    if (t == nullptr) {
      if (tracks_.size() < kMaxTracks) {
        tracks_.emplace_back();
        t = &tracks_.back();
      } else {
        // Full: take the place of the aircraft reported longest ago that
        // isn't in this fetch (it is about to drop out anyway).
        for (Track& old : tracks_) {
          if (old.last_seen_ms != now_ms && (t == nullptr || static_cast<int32_t>(old.last_seen_ms - t->last_seen_ms) < 0)) {
            t = &old;
          }
        }
        if (t == nullptr) continue;
        *t = Track();
      }
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

bool TrafficStore::remove(const char* hex) {
  std::lock_guard<std::mutex> lock(mu_);
  const size_t n = tracks_.size();
  tracks_.erase(std::remove_if(tracks_.begin(), tracks_.end(),
                               [hex](const Track& t) { return std::strcmp(t.ac.hex, hex) == 0; }),
                tracks_.end());
  if (tracks_.size() == n) return false;
  ++version_;
  return true;
}

}  // namespace core
