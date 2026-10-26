#include "core/adsb.h"

#include <cJSON.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace core::adsb {

namespace {

bool getNum(const cJSON* obj, const char* key, double* out) {
  const cJSON* v = cJSON_GetObjectItemCaseSensitive(obj, key);
  if (cJSON_IsNumber(v)) {
    *out = v->valuedouble;
    return true;
  }
  return false;
}

/** Copies a JSON string field, trimming trailing spaces (callsigns are padded). */
void getStr(const cJSON* obj, const char* key, char* out, size_t out_len) {
  out[0] = '\0';
  const cJSON* v = cJSON_GetObjectItemCaseSensitive(obj, key);
  if (!cJSON_IsString(v) || v->valuestring == nullptr) return;
  size_t n = strnlen(v->valuestring, out_len - 1);
  while (n > 0 && v->valuestring[n - 1] == ' ') --n;
  std::memcpy(out, v->valuestring, n);
  out[n] = '\0';
}

bool isEmergencySquawk(const char* sq) {
  return std::strcmp(sq, "7500") == 0 || std::strcmp(sq, "7600") == 0 ||
         std::strcmp(sq, "7700") == 0;
}

}  // namespace

const char* sourceName(DataSource src) {
  switch (src) {
    case DataSource::AirplanesLive: return "airplanes.live";
    case DataSource::AdsbLol: return "adsb.lol";
    case DataSource::AdsbFi:
    default: return "adsb.fi";
  }
}

std::string buildUrl(DataSource src, double lat, double lon, double radius_km) {
  double nm = radius_km / kKmPerNm;
  if (nm > kMaxQueryRadiusNm) nm = kMaxQueryRadiusNm;
  if (nm < 1.0) nm = 1.0;
  char buf[160];
  switch (src) {
    case DataSource::AirplanesLive:
      std::snprintf(buf, sizeof(buf), "https://api.airplanes.live/v2/point/%.5f/%.5f/%.0f", lat, lon,
                    std::ceil(nm));
      break;
    case DataSource::AdsbLol:
      std::snprintf(buf, sizeof(buf), "https://api.adsb.lol/v2/lat/%.5f/lon/%.5f/dist/%.0f", lat,
                    lon, std::ceil(nm));
      break;
    case DataSource::AdsbFi:
    default:
      std::snprintf(buf, sizeof(buf), "https://opendata.adsb.fi/api/v3/lat/%.5f/lon/%.5f/dist/%.1f",
                    lat, lon, nm);
      break;
  }
  return buf;
}

bool parse(const char* json, size_t len, const ParseOptions& opt, std::vector<Aircraft>* out) {
  out->clear();
  cJSON* root = cJSON_ParseWithLength(json, len);
  if (root == nullptr) return false;

  const cJSON* list = cJSON_GetObjectItemCaseSensitive(root, "ac");
  if (!cJSON_IsArray(list)) list = cJSON_GetObjectItemCaseSensitive(root, "aircraft");
  if (!cJSON_IsArray(list)) {
    // A valid response with no traffic may omit the array or send null.
    const bool ok = cJSON_IsObject(root);
    cJSON_Delete(root);
    return ok;
  }

  out->reserve(cJSON_GetArraySize(list));
  const cJSON* p = nullptr;
  cJSON_ArrayForEach(p, list) {
    Aircraft a;
    if (!getNum(p, "lat", &a.lat) || !getNum(p, "lon", &a.lon)) continue;

    const cJSON* alt_baro = cJSON_GetObjectItemCaseSensitive(p, "alt_baro");
    double v = 0;
    if (cJSON_IsString(alt_baro) && std::strcmp(alt_baro->valuestring, "ground") == 0) {
      a.on_ground = true;
      a.alt_ft = 0;
      a.alt_valid = true;
    } else if (getNum(p, "alt_baro", &v) || getNum(p, "alt_geom", &v)) {
      a.alt_ft = static_cast<int32_t>(std::lround(v));
      a.alt_valid = true;
    }

    if (a.on_ground && !opt.include_ground) continue;
    if (!a.on_ground && a.alt_valid && (a.alt_ft < opt.min_alt_ft || a.alt_ft > opt.max_alt_ft)) {
      continue;
    }

    // Track is the direction of motion (used for dead reckoning); heading is
    // where the nose points (used for the symbol). Each falls back to the other.
    double hdg = 0;
    const bool have_hdg = getNum(p, "true_heading", &hdg) || getNum(p, "mag_heading", &hdg);
    if (getNum(p, "track", &v)) {
      a.track_deg = static_cast<float>(v);
    } else if (have_hdg) {
      a.track_deg = static_cast<float>(hdg);
    }
    a.heading_deg = have_hdg ? static_cast<float>(hdg) : a.track_deg;

    if (getNum(p, "gs", &v) || getNum(p, "tas", &v) || getNum(p, "ias", &v)) {
      a.gs_kt = static_cast<float>(v);
    }
    if (getNum(p, "baro_rate", &v) || getNum(p, "geom_rate", &v)) {
      a.vrate_fpm = static_cast<int32_t>(std::lround(v));
    }
    if (getNum(p, "seen_pos", &v)) {
      a.seen_pos_s = static_cast<float>(v < 0 ? 0 : (v > 60 ? 60 : v));
    }

    getStr(p, "hex", a.hex, sizeof(a.hex));
    getStr(p, "flight", a.callsign, sizeof(a.callsign));
    getStr(p, "r", a.reg, sizeof(a.reg));
    getStr(p, "t", a.type, sizeof(a.type));
    getStr(p, "squawk", a.squawk, sizeof(a.squawk));
    getStr(p, "category", a.category, sizeof(a.category));

    char emergency[16];
    getStr(p, "emergency", emergency, sizeof(emergency));
    a.emergency = isEmergencySquawk(a.squawk) ||
                  (emergency[0] != '\0' && std::strcmp(emergency, "none") != 0);

    if (a.hex[0] == '\0') continue;  // can't track it across fetches
    out->push_back(a);
  }

  cJSON_Delete(root);
  return true;
}

}  // namespace core::adsb
