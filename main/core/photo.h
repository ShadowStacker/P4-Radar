#pragma once

#include <cstddef>
#include <string>

/** Aircraft photos from planespotters.net (by ICAO hex). */
namespace core::photo {

/** Photo lookup for an aircraft (hex in lower case). */
std::string lookupUrl(const char* hex);

struct Info {
  std::string image_url;     // JPEG thumbnail (the larger one when offered)
  std::string photographer;  // must be credited next to the photo
  std::string link;          // the photo's page
};

/** Reads the lookup response; false when there's no photo. */
bool parse(const char* json, size_t len, Info* out);

}  // namespace core::photo
