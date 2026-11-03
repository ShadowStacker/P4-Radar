#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace core::geocode {

/**
 * Place-name search: cities, states, countries and more, from OpenStreetMap's
 * Nominatim (https://nominatim.openstreetmap.org). Its usage policy asks for
 * an identifying User-Agent and at most one request a second; the radar only
 * asks when someone searches.
 */
struct Result {
  std::string name;    // "Bend"
  std::string detail;  // "Deschutes County, Oregon, United States"
  std::string kind;    // "city", "state", "country", ... (Nominatim's addresstype)
  double lat = 0;
  double lon = 0;
  double radius_km = 25;  // a zoom that shows the whole place (from its bounding box)
};

constexpr int kMaxResults = 6;

/** Percent-encodes `s` for a URL query value (UTF-8 bytes kept as %XX). */
std::string urlEncode(const std::string& s);

/** The search URL for what the user typed. */
std::string searchUrl(const std::string& query);

/**
 * Reads a Nominatim jsonv2 response. False if it isn't a JSON array; an
 * empty array is a valid "nothing found".
 */
bool parse(const char* json, size_t len, std::vector<Result>* out);

/** Radius (km) that fits a bounding box, kept to what the radar can show. */
double radiusForBox(double south, double north, double west, double east);

}  // namespace core::geocode
