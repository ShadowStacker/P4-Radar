#pragma once

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

#include "core/geo.h"
#include "core/traffic.h"

/**
 * Today's traffic near home: how many different aircraft, the busiest
 * moment, the closest pass, the highest and fastest, military and emergency
 * aircraft, and favorites seen. Saved as a small text file per day.
 */
namespace core::daylog {

struct Record {
  char label[12] = "";  // callsign, else registration, else hex
  char type[5] = "";
  float value = 0;      // km (closest), ft (highest) or kt (fastest)
  int32_t alt_ft = 0;   // closest pass: altitude
  int16_t minute = -1;  // local time of day; -1 = none yet
};

struct Day {
  int date = 0;                 // yyyymmdd, local
  std::vector<uint32_t> seen;   // ICAO addresses, sorted
  std::vector<uint32_t> military, emergencies;
  std::vector<std::string> favorites;  // labels, in the order first seen
  int peak = 0;                 // most aircraft at once
  int16_t peak_minute = -1;
  Record closest, highest, fastest;

  int count() const { return static_cast<int>(seen.size()); }
};

/** Local date as yyyymmdd. */
int dateOf(const std::tm& t);

/**
 * Adds one fetch's aircraft (airborne ones within `radius_km` of home count).
 * `is_favorite(hex)` marks saved aircraft. Starts a new day when `date` moves
 * on; returns true if it did (the caller saves the finished day first).
 */
bool observe(Day* day, const std::vector<Aircraft>& aircraft, const LatLon& home, double radius_km, int date,
             int minute, bool (*is_favorite)(const char* hex));

/** "date = 2026-10-29\naircraft = 143\n..." with every seen address at the end. */
std::string serialize(const Day& d);
bool parse(const std::string& text, Day* out);
/** File name for a day: "2026-10-29.txt". */
std::string fileName(int date);
/** "Oct 29: 143 aircraft, closest N123AB 0.4 km" (distance in the caller's units). */
std::string summary(const Day& d, const char* closest_distance);

}  // namespace core::daylog
