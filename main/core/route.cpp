#include "core/route.h"

#include <cJSON.h>

#include <cctype>
#include <cstdio>
#include <cstring>

namespace core::route {

namespace {

const char* kBase = "https://api.adsbdb.com/v0/";

const char* str(const cJSON* obj, const char* key) {
  const cJSON* v = cJSON_GetObjectItemCaseSensitive(obj, key);
  return cJSON_IsString(v) && v->valuestring != nullptr ? v->valuestring : "";
}

void copy(char* dst, size_t n, const char* src) { std::snprintf(dst, n, "%s", src); }

/** Callsigns are A-Z/0-9; anything else is dropped so it can't break the URL. */
std::string clean(const char* s) {
  std::string out;
  for (; s != nullptr && *s; ++s) {
    if (std::isalnum(static_cast<unsigned char>(*s))) out.push_back(static_cast<char>(std::toupper(*s)));
  }
  return out;
}

void parseAirport(const cJSON* a, Airport* out) {
  copy(out->code, sizeof(out->code), str(a, "iata_code")[0] ? str(a, "iata_code") : str(a, "icao_code"));
  copy(out->city, sizeof(out->city), str(a, "municipality")[0] ? str(a, "municipality") : str(a, "name"));
}

void parseAirline(const cJSON* al, Info* out) {
  if (cJSON_IsObject(al) && str(al, "name")[0]) copy(out->airline, sizeof(out->airline), str(al, "name"));
}

}  // namespace

std::string aircraftUrl(const char* hex, const char* callsign) {
  std::string url = std::string(kBase) + "aircraft/" + clean(hex);
  const std::string cs = clean(callsign);
  if (!cs.empty()) url += "?callsign=" + cs;
  return url;
}

std::string lookupUrl(const char* q) {
  // Registrations keep their dash (adsbdb matches "EI-DAC", not "EIDAC").
  std::string out;
  for (const char* c = q; c != nullptr && *c; ++c) {
    if (std::isalnum(static_cast<unsigned char>(*c)) || *c == '-') out.push_back(static_cast<char>(std::toupper(*c)));
  }
  return std::string(kBase) + "aircraft/" + out;
}

std::string callsignUrl(const char* callsign) { return std::string(kBase) + "callsign/" + clean(callsign); }

std::string airlineUrl(const char* icao_prefix) { return std::string(kBase) + "airline/" + clean(icao_prefix); }

std::string airlinePrefix(const char* callsign) {
  const std::string cs = clean(callsign);
  // Airline callsigns: three letters then a digit (RYR12AB, BAW1425). N123AB,
  // PHABC and similar are registrations.
  if (cs.size() >= 4 && std::isalpha(static_cast<unsigned char>(cs[0])) &&
      std::isalpha(static_cast<unsigned char>(cs[1])) && std::isalpha(static_cast<unsigned char>(cs[2])) &&
      std::isdigit(static_cast<unsigned char>(cs[3]))) {
    return cs.substr(0, 3);
  }
  return "";
}

bool parse(const char* json, size_t len, Info* out) {
  cJSON* root = cJSON_ParseWithLength(json, len);
  if (root == nullptr) return false;
  bool any = false;
  const cJSON* resp = cJSON_GetObjectItemCaseSensitive(root, "response");
  if (cJSON_IsObject(resp)) {
    const cJSON* ac = cJSON_GetObjectItemCaseSensitive(resp, "aircraft");
    if (cJSON_IsObject(ac)) {
      any = true;
      const char* mfr = str(ac, "manufacturer");
      const char* type = str(ac, "type");
      if (mfr[0] && type[0] && std::strstr(type, mfr) != type) {
        std::snprintf(out->model, sizeof(out->model), "%s %s", mfr, type);
      } else {
        copy(out->model, sizeof(out->model), type[0] ? type : mfr);
      }
      copy(out->owner, sizeof(out->owner), str(ac, "registered_owner"));
      copy(out->registration, sizeof(out->registration), str(ac, "registration"));
      copy(out->icao_type, sizeof(out->icao_type), str(ac, "icao_type"));
      copy(out->mode_s, sizeof(out->mode_s), str(ac, "mode_s"));
      for (char* c = out->mode_s; *c; ++c) *c = static_cast<char>(std::tolower(static_cast<unsigned char>(*c)));
    }
    const cJSON* fr = cJSON_GetObjectItemCaseSensitive(resp, "flightroute");
    if (cJSON_IsObject(fr)) {
      any = true;
      parseAirline(cJSON_GetObjectItemCaseSensitive(fr, "airline"), out);
      const cJSON* o = cJSON_GetObjectItemCaseSensitive(fr, "origin");
      const cJSON* d = cJSON_GetObjectItemCaseSensitive(fr, "destination");
      if (cJSON_IsObject(o) && cJSON_IsObject(d)) {
        parseAirport(o, &out->origin);
        parseAirport(d, &out->destination);
        out->has_route = out->origin.code[0] != '\0' && out->destination.code[0] != '\0';
      }
    }
  } else if (cJSON_IsArray(resp) && cJSON_GetArraySize(resp) > 0) {  // /airline/<icao>
    any = true;
    parseAirline(cJSON_GetArrayItem(resp, 0), out);
  }
  cJSON_Delete(root);
  return any;
}

}  // namespace core::route
