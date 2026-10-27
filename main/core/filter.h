#pragma once

#include <cstdint>

#include "core/settings.h"
#include "core/traffic.h"

namespace core {

/** Aircraft kinds, grouped from the ADS-B emitter category. */
enum CategoryBit : uint8_t {
  kCatLight = 1 << 0,       // A1 light aircraft, B4 ultralights
  kCatSmall = 1 << 1,       // A2 small (business jets, turboprops)
  kCatAirliner = 1 << 2,    // A3 large (A320/B737 class)
  kCatHeavy = 1 << 3,       // A4 B757, A5 heavy (widebodies)
  kCatFast = 1 << 4,        // A6 high performance (fighters)
  kCatRotor = 1 << 5,       // A7 helicopters
  kCatOther = 1 << 6,       // B1 gliders, B2 balloons, B6 drones, B3/B7 ...
  kCatUnknown = 1 << 7,     // no category broadcast
};
constexpr int kCategoryCount = 8;
constexpr uint16_t kSpeedNoLimit = 1000;  // max_speed_kt at or above this = no limit

CategoryBit categoryOf(const char* category);
const char* categoryName(CategoryBit c);

/** True if `type` (e.g. "B738") matches a comma/space separated list like "B738, A32*". */
bool typeMatches(const char* type, const char* filter);

/** The user's display filters (category, military, ground, altitude, speed, type). */
bool passesFilters(const Aircraft& a, const Settings& s);

/** True if any filter would hide something (for the "N of M" status). */
bool filtersActive(const Settings& s);

}  // namespace core
