#include "core/geocode.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cctype>
#include <cstring>

#include "cJSON.h"
#include "core/geo.h"
#include "core/viewport.h"

namespace core::geocode {

namespace {

double number(const cJSON* item) {
  if (cJSON_IsNumber(item)) return item->valuedouble;
  if (cJSON_IsString(item) && item->valuestring != nullptr && item->valuestring[0] != '\0') {
    char* end = nullptr;
    const double v = std::strtod(item->valuestring, &end);
    if (end != nullptr && *end == '\0') return v;
  }
  return NAN;
}

std::string text(const cJSON* obj, const char* key) {
  const cJSON* v = cJSON_GetObjectItemCaseSensitive(obj, key);
  return cJSON_IsString(v) && v->valuestring != nullptr ? v->valuestring : "";
}

std::string trim(std::string s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
  size_t i = 0;
  while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
  return s.substr(i);
}

}  // namespace

std::string urlEncode(const std::string& s) {
  static const char* kHex = "0123456789ABCDEF";
  std::string out;
  for (unsigned char c : s) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out.push_back(static_cast<char>(c));
    } else if (c == ' ') {
      out += "%20";
    } else {
      out.push_back('%');
      out.push_back(kHex[c >> 4]);
      out.push_back(kHex[c & 15]);
    }
  }
  return out;
}

std::string searchUrl(const std::string& query) {
  return "https://nominatim.openstreetmap.org/search?format=jsonv2&accept-language=en&limit=" +
         std::to_string(kMaxResults) + "&q=" + urlEncode(trim(query));
}

double radiusForBox(double south, double north, double west, double east) {
  const double mid_lat = (south + north) / 2.0;
  const double height = distanceKm({south, (west + east) / 2.0}, {north, (west + east) / 2.0});
  const double width = distanceKm({mid_lat, west}, {mid_lat, east});
  const double r = std::max(height, width) / 2.0 * 1.1;  // a little margin around it
  return std::min(Viewport::kMaxRadiusKm, std::max(5.0, std::isfinite(r) ? r : 25.0));
}

bool parse(const char* json, size_t len, std::vector<Result>* out) {
  out->clear();
  cJSON* root = cJSON_ParseWithLength(json, len);
  if (root == nullptr) return false;
  if (!cJSON_IsArray(root)) {
    cJSON_Delete(root);
    return false;
  }
  const cJSON* item = nullptr;
  cJSON_ArrayForEach(item, root) {
    if (static_cast<int>(out->size()) >= kMaxResults) break;
    Result r;
    r.lat = number(cJSON_GetObjectItemCaseSensitive(item, "lat"));
    r.lon = number(cJSON_GetObjectItemCaseSensitive(item, "lon"));
    if (!std::isfinite(r.lat) || !std::isfinite(r.lon) || std::fabs(r.lat) > 90 || std::fabs(r.lon) > 180) continue;
    const std::string display = text(item, "display_name");
    r.name = text(item, "name");
    const size_t comma = display.find(',');
    if (r.name.empty()) r.name = trim(display.substr(0, comma));
    // The rest of the full name says where it is: "Deschutes County, Oregon, United States".
    r.detail = comma == std::string::npos ? "" : trim(display.substr(comma + 1));
    r.kind = text(item, "addresstype");
    if (r.kind.empty()) r.kind = text(item, "type");
    std::replace(r.kind.begin(), r.kind.end(), '_', ' ');
    const cJSON* box = cJSON_GetObjectItemCaseSensitive(item, "boundingbox");  // [south, north, west, east]
    if (cJSON_IsArray(box) && cJSON_GetArraySize(box) == 4) {
      const double s = number(cJSON_GetArrayItem(box, 0)), n = number(cJSON_GetArrayItem(box, 1));
      const double w = number(cJSON_GetArrayItem(box, 2)), e = number(cJSON_GetArrayItem(box, 3));
      if (std::isfinite(s) && std::isfinite(n) && std::isfinite(w) && std::isfinite(e)) r.radius_km = radiusForBox(s, n, w, e);
    }
    out->push_back(std::move(r));
  }
  cJSON_Delete(root);
  return true;
}

}  // namespace core::geocode
