#include "fake_traffic.h"

#include <cmath>
#include <cstdio>

namespace sim {

namespace {

struct TypeInfo {
  const char* type;
  const char* category;
  double min_gs, max_gs;
};

const TypeInfo kTypes[] = {
    {"A320", "A3", 250, 470}, {"B738", "A3", 250, 470}, {"E190", "A3", 240, 450},
    {"A21N", "A3", 260, 470}, {"B77W", "A5", 300, 500}, {"A359", "A5", 300, 500},
    {"B789", "A5", 300, 500}, {"C172", "A1", 90, 120},  {"PC12", "A1", 150, 260},
    {"EC35", "A7", 90, 130},  {"B752", "A4", 280, 470},
};
const char* kAirlines[] = {"KLM", "EZY", "RYR", "DLH", "BAW", "TRA", "UAE", "AFR", "SAS", "TAP"};

}  // namespace

FakeTraffic::FakeTraffic(core::LatLon home, uint32_t seed) : home_(home), rng_(seed) {
  planes_.resize(46);
  for (size_t i = 0; i < planes_.size(); ++i) spawn(planes_[i], i < 12);

  // A handful of military aircraft (dbFlags bit 0 in the feed).
  const char* kMil[][3] = {{"RCH471", "C17", "A5"}, {"NATO05", "E3TF", "A5"}, {"DUKE21", "H60", "A7"},
                           {"VIPER1", "F16", "A2"}};
  for (int i = 0; i < 4; ++i) {
    Plane& p = planes_[20 + i];
    p.flight = kMil[i][0];
    p.type = kMil[i][1];
    p.category = kMil[i][2];
    p.reg = "";
    p.military = true;
    p.pos = core::destination(home_, 60.0 + i * 80.0, 12.0 + i * 6.0);
  }

  // A few arrivals lined up on a north-south final near home.
  for (int i = 0; i < 3; ++i) {
    Plane& p = planes_[i];
    p.pos = core::destination(home_, 0.0, 9.0 + i * 11.0);
    p.pos = core::destination(p.pos, 270.0, 6.0);
    p.track = 182.0;
    p.turn = 0;
    p.gs = 150 + i * 25;
    p.alt = 1200 + i * 1900;
    p.vrate = -750;
  }
}

void FakeTraffic::spawn(Plane& p, bool near_home) {
  std::uniform_real_distribution<double> u(0.0, 1.0);
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%06x", static_cast<unsigned>(rng_() & 0xFFFFFF));
  p.hex = buf;
  p.military = false;
  const TypeInfo& t = kTypes[rng_() % (sizeof(kTypes) / sizeof(kTypes[0]))];
  p.type = t.type;
  p.category = t.category;
  if (t.category[1] == '1' || t.category[1] == '7') {
    std::snprintf(buf, sizeof(buf), "PH%c%c%c", 'A' + static_cast<char>(rng_() % 26),
                  'A' + static_cast<char>(rng_() % 26), 'A' + static_cast<char>(rng_() % 26));
    p.flight = buf;
    p.reg = std::string(buf).insert(2, "-");
  } else {
    std::snprintf(buf, sizeof(buf), "%s%u", kAirlines[rng_() % 10], static_cast<unsigned>(10 + rng_() % 1990));
    p.flight = buf;
    std::snprintf(buf, sizeof(buf), "PH-%c%c%c", 'A' + static_cast<char>(rng_() % 26),
                  'A' + static_cast<char>(rng_() % 26), 'A' + static_cast<char>(rng_() % 26));
    p.reg = buf;
  }
  const double dist = near_home ? 4 + u(rng_) * 35 : 20 + u(rng_) * 130;
  p.pos = core::destination(home_, u(rng_) * 360.0, dist);
  p.track = u(rng_) * 360.0;
  p.gs = t.min_gs + u(rng_) * (t.max_gs - t.min_gs);
  if (t.category[1] == '1' || t.category[1] == '7') {
    p.alt = 800 + u(rng_) * 3500;
  } else {
    p.alt = 3000 + u(rng_) * 36000;
  }
  p.vrate = u(rng_) < 0.4 ? (u(rng_) - 0.5) * 3600 : 0;
  p.turn = u(rng_) < 0.3 ? (u(rng_) - 0.5) * 1.2 : 0;  // deg/s
  std::snprintf(buf, sizeof(buf), "%04o", static_cast<unsigned>(rng_() % 4096));
  p.squawk = buf;
  if (p.squawk == "7500" || p.squawk == "7600" || p.squawk == "7700") p.squawk = "2000";
}

void FakeTraffic::advance(double dt_s) {
  for (Plane& p : planes_) {
    p.track = std::fmod(p.track + p.turn * dt_s + 360.0, 360.0);
    p.pos = core::destination(p.pos, p.track, p.gs * core::kKmPerNm / 3600.0 * dt_s);
    p.alt = std::fmax(500.0, std::fmin(41000.0, p.alt + p.vrate / 60.0 * dt_s));
    if (core::distanceKm(home_, p.pos) > 220) spawn(p, false);
  }
}

void FakeTraffic::recallMilitary() {
  int i = 0;
  for (Plane& p : planes_) {
    if (!p.military) continue;
    p.pos = core::destination(home_, 40.0 + i * 95.0, 8.0 + i * 3.0);
    ++i;
  }
}

void FakeTraffic::setEmergency(bool on) {
  if (planes_.size() <= 5) return;
  Plane& p = planes_[5];
  p.squawk = on ? "7700" : "2000";
  if (on) {
    p.pos = core::destination(home_, 300.0, 9.0);
    p.track = 120.0;
    p.turn = 0;
  }
}

std::string FakeTraffic::traceJson(const std::string& hex) const {
  const Plane* plane = nullptr;
  for (const Plane& p : planes_) {
    if (p.hex == hex) plane = &p;
  }
  if (plane == nullptr) return "";

  // Walk backwards from the current position, then emit oldest first.
  struct Pt {
    double t;
    core::LatLon pos;
    int alt;
    int flags;
  };
  std::vector<Pt> pts;
  const double kStep = 15.0, kMinutes = 28.0;
  const int n = static_cast<int>(kMinutes * 60 / kStep);
  core::LatLon pos = plane->pos;
  double track = plane->track;
  for (int i = 0; i <= n; ++i) {
    const double t_ago = i * kStep;
    const double frac = 1.0 - static_cast<double>(i) / n;  // 1 now, 0 at takeoff
    pts.push_back({-t_ago, pos, static_cast<int>(plane->alt * std::min(1.0, frac * 1.6)), 0});
    // Earlier in the flight the aircraft was turning onto its current course.
    const double turn = i < n / 3 ? 0.0 : 0.22 * std::sin(i * 0.035);
    track = std::fmod(track - turn * kStep + 360.0, 360.0);
    const double gs = plane->gs * (0.55 + 0.45 * frac);
    pos = core::destination(pos, track + 180.0, gs * core::kKmPerNm / 3600.0 * kStep);
  }
  pts.back().flags = 2;  // takeoff: start of the current leg
  pts.back().alt = -1;   // on the ground
  // An earlier leg somewhere else that must not be drawn.
  const core::LatLon earlier = core::destination(pts.back().pos, 45.0, 300.0);
  for (int i = 0; i < 20; ++i) {
    pts.push_back({-(n * kStep) - 3600.0 - i * 30.0, core::destination(earlier, 90.0, i * 4.0), 20000, 0});
  }

  const double now = 1700000000.0;
  const double start = now + pts.back().t;
  std::string out;
  char buf[160];
  std::snprintf(buf, sizeof(buf), "{\"icao\":\"%s\",\"r\":\"%s\",\"t\":\"%s\",\"dbFlags\":0,\"timestamp\":%.3f,\"trace\":[",
                plane->hex.c_str(), plane->reg.c_str(), plane->type.c_str(), start);
  out = buf;
  for (size_t k = pts.size(); k-- > 0;) {
    const Pt& p = pts[k];
    char alt[16];
    if (p.alt < 0) std::snprintf(alt, sizeof(alt), "\"ground\"");
    else std::snprintf(alt, sizeof(alt), "%d", p.alt);
    std::snprintf(buf, sizeof(buf), "%s[%.2f,%.6f,%.6f,%s,%.1f,%.1f,%d,null,%s,\"adsb_icao\",null,null,null,null]",
                  k == pts.size() - 1 ? "" : ",", p.t - pts.back().t, p.pos.lat, p.pos.lon, alt, plane->gs,
                  plane->track, p.flags, k == 0 ? "{\"flight\":\"TEST\"}" : "null");
    out += buf;
  }
  out += "]}";
  return out;
}

std::string FakeTraffic::json(const core::LatLon& center, double radius_km) const {
  std::string out = "{\"now\":1700000000000,\"msg\":\"No error\",\"ac\":[";
  bool first = true;
  char buf[512];
  for (const Plane& p : planes_) {
    if (core::distanceKm(center, p.pos) > radius_km) continue;
    std::snprintf(buf, sizeof(buf),
                  "%s{\"hex\":\"%s\",\"type\":\"adsb_icao\",\"flight\":\"%-8s\",\"r\":\"%s\",\"t\":\"%s\","
                  "\"alt_baro\":%d,\"alt_geom\":%d,\"gs\":%.1f,\"track\":%.1f,\"baro_rate\":%d,"
                  "\"squawk\":\"%s\",\"emergency\":\"none\",\"category\":\"%s\",\"dbFlags\":%d,\"lat\":%.6f,\"lon\":%.6f,"
                  "\"seen_pos\":0.4,\"seen\":0.2,\"rssi\":-21.5}",
                  first ? "" : ",", p.hex.c_str(), p.flight.c_str(), p.reg.c_str(), p.type.c_str(),
                  static_cast<int>(p.alt), static_cast<int>(p.alt + 150), p.gs, p.track,
                  static_cast<int>(p.vrate), p.squawk.c_str(), p.category.c_str(), p.military ? 1 : 0,
                  p.pos.lat, p.pos.lon);
    out += buf;
    first = false;
  }
  out += "],\"total\":0}";
  return out;
}

}  // namespace sim
