#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/settings.h"

namespace core::trace {

/**
 * Flight history ("trace") of one aircraft, as published by readsb-based
 * aggregators for their tar1090 globe maps:
 *   https://<host>/data/traces/<last 2 hex digits>/trace_full_<hex>.json
 * The file holds up to a day of positions; we keep the current flight.
 */
struct Point {
  float lat;
  float lon;
  int32_t alt_ft;  // 0 on the ground
  float t;         // seconds after Trace::timestamp
  uint8_t flags;   // kGround, kGapBefore
};

constexpr uint8_t kGround = 1;
constexpr uint8_t kGapBefore = 2;  // don't join this point to the previous one

/** Long enough for a transatlantic flight at one point every ~30 s. */
constexpr size_t kMaxPoints = 1500;
/** Older points (earlier legs, yesterday) are dropped. */
constexpr float kMaxHours = 14.0f;
/** A silence this long splits the line (coverage holes). */
constexpr float kGapSeconds = 30.0f * 60.0f;

struct Trace {
  double timestamp = 0;  // unix seconds of t = 0
  std::vector<Point> points;

  float durationS() const { return points.size() < 2 ? 0 : points.back().t - points.front().t; }
};

/** Hosts serving traces, the one matching `src` first. */
std::vector<std::string> hostsFor(DataSource src);
std::string buildUrl(const std::string& host, const char* hex, bool full = true);

/**
 * Parses a (decompressed) trace file. Keeps the last leg (the readsb "new
 * leg" flag marks takeoffs after a landing), at most kMaxHours of it, thinned
 * to kMaxPoints. Returns false if the JSON has no usable trace.
 */
bool parse(const char* json, size_t len, Trace* out);

}  // namespace core::trace
