#pragma once

#include <cstddef>
#include <string>

namespace core::route {

/**
 * Airline, route and airframe details for one aircraft, from adsbdb.com
 * (https://www.adsbdb.com): GET /v0/aircraft/<hex>?callsign=<callsign>.
 * Routes come from a crowd-sourced callsign database, so they can be stale.
 */
struct Airport {
  char code[5] = "";   // IATA if known, else ICAO
  char city[32] = "";  // municipality, else airport name
};

struct Info {
  char airline[40] = "";  // "Ryanair"
  char owner[40] = "";    // registered owner (used when there's no airline)
  char model[40] = "";    // "Boeing 737-8AS"
  bool has_route = false;
  Airport origin;
  Airport destination;
};

std::string aircraftUrl(const char* hex, const char* callsign);
std::string callsignUrl(const char* callsign);
std::string airlineUrl(const char* icao_prefix);

/** ICAO airline prefix of a callsign like "RYR12AB" ("RYR"), or "" for registrations. */
std::string airlinePrefix(const char* callsign);

/**
 * Merges an adsbdb response into `out` (aircraft, flightroute or airline
 * array). Returns false if the JSON is malformed or carries none of them.
 */
bool parse(const char* json, size_t len, Info* out);

}  // namespace core::route
