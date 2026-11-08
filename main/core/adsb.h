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

/**
 * The URLs to try for your own receiver, from what was typed in settings
 * ("192.168.1.50", "pi.local:8080", "http://host/tar1090/"...). With a path
 * to a .json file, just that; otherwise the places readsb/tar1090,
 * dump1090(-fa) and SkyAware serve aircraft.json, most common first.
 * Empty if nothing usable was typed.
 */
std::vector<std::string> localUrls(const char* setting);

/** Keeps only the aircraft within radius_km of center (a receiver reports everything it hears). */
void keepWithin(std::vector<Aircraft>* list, const LatLon& center, double radius_km);

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

/**
 * Drops radar-only (TIS-B, "~" address) targets that are the same aircraft as
 * one reporting with its own address: within 2 km and 500 ft of it, both moved
 * on to the same moment (radar positions can be tens of seconds old).
 * Returns how many were dropped.
 */
size_t dropRadarDuplicates(std::vector<Aircraft>* list);

}  // namespace core::adsb
