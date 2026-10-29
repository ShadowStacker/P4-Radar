#pragma once

#include <string>

#include "core/route.h"

/**
 * adsbdb lookups kept on the SD card, so aircraft seen before show their
 * airline and route at once (and offline). Two small text files per lookup:
 * the airframe (owner, model, registration, type) by ICAO hex, and the
 * airline and route by callsign. Answers that found nothing are kept too,
 * for a shorter time.
 */
namespace core::route {

/** "lookups/aircraft/a1/a1b2c3.txt" (relative to the card). "" for a bad hex. */
std::string airframePath(const char* hex);
/** "lookups/routes/RYR/RYR12AB.txt". "" for an empty or odd callsign. */
std::string routePath(const char* callsign);

std::string serializeAirframe(const Info& in, int date);
std::string serializeRoute(const Info& in, int date);

/**
 * Merges the fields in a cache file into *out. False if the file is
 * damaged; `date` (yyyymmdd) is when it was saved, `empty` whether it holds
 * no information (a lookup that found nothing).
 */
bool parseCached(const std::string& text, Info* out, int* date, bool* empty);

/** Whether a file saved on `saved` (yyyymmdd) is still good on `today`. */
bool airframeFresh(int saved, int today, bool empty);
bool routeFresh(int saved, int today, bool empty);

}  // namespace core::route
