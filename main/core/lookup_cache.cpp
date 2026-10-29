#include "core/lookup_cache.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/sun.h"

namespace core::route {

namespace {

constexpr int kAirframeDays = 180, kRouteDays = 30, kEmptyDays = 3;

int64_t dayNumber(int yyyymmdd) { return sun::daysFromCivil(yyyymmdd / 10000, yyyymmdd / 100 % 100, yyyymmdd % 100); }

bool fresh(int saved, int today, int days) {
  if (saved <= 0 || today <= 0) return false;
  const int64_t age = dayNumber(today) - dayNumber(saved);
  return age >= 0 && age < days;
}

void line(std::string* out, const char* key, const char* value) {
  if (value[0] == '\0') return;
  *out += key;
  *out += " = ";
  for (const char* p = value; *p; ++p) out->push_back(*p == '\n' || *p == '\r' ? ' ' : *p);
  *out += "\n";
}

template <size_t N>
void set(char (&dst)[N], const std::string& v) {
  std::snprintf(dst, N, "%s", v.c_str());
}

}  // namespace

std::string airframePath(const char* hex) {
  const size_t n = std::strlen(hex);
  if (n < 2 || n > 7) return "";
  std::string h;
  for (size_t i = 0; i < n; ++i) {
    const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(hex[i])));
    if (!std::isxdigit(static_cast<unsigned char>(c))) return "";  // also "~" (non-ICAO) addresses
    h.push_back(c);
  }
  return "lookups/aircraft/" + h.substr(0, 2) + "/" + h + ".txt";
}

std::string routePath(const char* callsign) {
  std::string c;
  for (const char* p = callsign; *p; ++p) {
    if (!std::isalnum(static_cast<unsigned char>(*p))) return "";
    c.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(*p))));
  }
  if (c.empty() || c.size() > 8) return "";
  return "lookups/routes/" + c.substr(0, 3) + "/" + c + ".txt";
}

std::string serializeAirframe(const Info& in, int date) {
  std::string out = "saved = " + std::to_string(date) + "\n";
  line(&out, "registration", in.registration);
  line(&out, "type", in.icao_type);
  line(&out, "model", in.model);
  line(&out, "owner", in.owner);
  line(&out, "mode_s", in.mode_s);
  return out;
}

std::string serializeRoute(const Info& in, int date) {
  std::string out = "saved = " + std::to_string(date) + "\n";
  line(&out, "airline", in.airline);
  if (in.has_route) {
    line(&out, "from_code", in.origin.code);
    line(&out, "from_city", in.origin.city);
    line(&out, "to_code", in.destination.code);
    line(&out, "to_city", in.destination.city);
  }
  return out;
}

bool parseCached(const std::string& text, Info* out, int* date, bool* empty) {
  *date = 0;
  *empty = true;
  size_t start = 0;
  while (start < text.size()) {
    size_t end = text.find('\n', start);
    if (end == std::string::npos) end = text.size();
    const std::string l = text.substr(start, end - start);
    start = end + 1;
    const size_t eq = l.find(" = ");
    if (eq == std::string::npos) continue;
    const std::string k = l.substr(0, eq), v = l.substr(eq + 3);
    if (k == "saved") { *date = std::atoi(v.c_str()); continue; }
    *empty = false;
    if (k == "registration") set(out->registration, v);
    else if (k == "type") set(out->icao_type, v);
    else if (k == "model") set(out->model, v);
    else if (k == "owner") set(out->owner, v);
    else if (k == "mode_s") set(out->mode_s, v);
    else if (k == "airline") set(out->airline, v);
    else if (k == "from_code") { set(out->origin.code, v); out->has_route = true; }
    else if (k == "from_city") set(out->origin.city, v);
    else if (k == "to_code") { set(out->destination.code, v); out->has_route = true; }
    else if (k == "to_city") set(out->destination.city, v);
  }
  return *date > 19700101;
}

bool airframeFresh(int saved, int today, bool empty) { return fresh(saved, today, empty ? kEmptyDays * 10 : kAirframeDays); }
bool routeFresh(int saved, int today, bool empty) { return fresh(saved, today, empty ? kEmptyDays : kRouteDays); }

}  // namespace core::route
