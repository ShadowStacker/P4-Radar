#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "core/settings.h"
#include "core/traffic.h"

namespace core::adsb {

/** adsb.fi, airplanes.live and adsb.lol all cap point queries at 250 nm. */
constexpr double kMaxQueryRadiusNm = 250.0;

/** Builds the point-query URL for a data source. */
std::string buildUrl(DataSource src, double lat, double lon, double radius_km);

const char* sourceName(DataSource src);

/** What a find-an-aircraft query is. */
enum class FindBy : uint8_t { Callsign, Registration, Hex };
/**
 * URL that asks a data source for one aircraft anywhere, by callsign,
 * registration or ICAO hex (the answer is in the usual {"ac":[...]} form).
 * `value` is upper-cased and stripped of anything but letters, digits and
 * '-' (hex is lower-cased). "" if nothing is left.
 */
std::string findUrl(DataSource src, FindBy by, const std::string& value);

/** Callsign, registration and hex compared the way people type them: case, spaces and '-' don't matter. */
bool sameIdent(const char* a, const std::string& typed);

struct ParseOptions {
  bool include_ground = false;
  bool military_only = false;
  int32_t min_alt_ft = -2000;
  int32_t max_alt_ft = 100000;
};

/**
 * Parses a readsb-format response ({"ac":[...]} or {"aircraft":[...]}).
 * Returns false if the JSON is malformed; aircraft without a position are
 * skipped silently.
 */
bool parse(const char* json, size_t len, const ParseOptions& opt, std::vector<Aircraft>* out);

}  // namespace core::adsb
