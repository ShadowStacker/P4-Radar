#include "core/filter.h"

#include <cctype>
#include <cstring>

namespace core {

CategoryBit categoryOf(const char* c) {
  if (c == nullptr || c[0] == '\0') return kCatUnknown;
  if (c[0] == 'A') {
    switch (c[1]) {
      case '1': return kCatLight;
      case '2': return kCatSmall;
      case '3': return kCatAirliner;
      case '4':
      case '5': return kCatHeavy;
      case '6': return kCatFast;
      case '7': return kCatRotor;
      default: return kCatUnknown;  // A0 = "no information"
    }
  }
  if (c[0] == 'B') return c[1] == '4' ? kCatLight : (c[1] == '0' ? kCatUnknown : kCatOther);
  return kCatUnknown;  // C* surface vehicles are handled by the ground filter
}

const char* categoryName(CategoryBit c) {
  switch (c) {
    case kCatLight: return "Light aircraft";
    case kCatSmall: return "Small & business";
    case kCatAirliner: return "Airliners";
    case kCatHeavy: return "Heavies (757, widebody)";
    case kCatFast: return "Fast jets";
    case kCatRotor: return "Helicopters";
    case kCatOther: return "Gliders, balloons, drones";
    case kCatUnknown: return "Unknown category";
  }
  return "";
}

bool typeMatches(const char* type, const char* filter) {
  if (filter == nullptr || filter[0] == '\0') return true;
  const char* p = filter;
  while (*p) {
    while (*p == ',' || *p == ' ') ++p;
    if (!*p) break;
    char tok[12];
    size_t n = 0;
    while (*p && *p != ',' && *p != ' ' && n < sizeof(tok) - 1) {
      tok[n++] = static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
      ++p;
    }
    while (*p && *p != ',' && *p != ' ') ++p;  // skip an over-long token's tail
    tok[n] = '\0';
    if (n == 0) continue;
    const bool prefix = tok[n - 1] == '*';
    if (prefix) tok[--n] = '\0';
    if (prefix ? std::strncmp(type, tok, n) == 0 : std::strcmp(type, tok) == 0) return true;
  }
  return false;
}

bool passesFilters(const Aircraft& a, const Settings& s) {
  if (a.on_ground && !s.show_ground) return false;
  if (s.military_only && !a.military) return false;
  if (!a.on_ground && !(s.category_mask & categoryOf(a.category))) return false;
  if (!a.on_ground && a.alt_valid) {
    if (s.min_alt_ft > 0 && a.alt_ft < s.min_alt_ft) return false;
    if (s.max_alt_ft < 60000 && a.alt_ft > s.max_alt_ft) return false;
  }
  if (!a.on_ground) {
    if (s.min_speed_kt > 0 && a.gs_kt < s.min_speed_kt) return false;
    if (s.max_speed_kt < kSpeedNoLimit && a.gs_kt > s.max_speed_kt) return false;
  }
  if (s.type_filter[0] != '\0' && !typeMatches(a.type, s.type_filter)) return false;
  return true;
}

bool filtersActive(const Settings& s) {
  // Hiding ground traffic is the default, so it doesn't count as "filtering".
  return s.military_only || s.category_mask != 0xFF || s.min_alt_ft > 0 ||
         s.max_alt_ft < 60000 || s.min_speed_kt > 0 || s.max_speed_kt < kSpeedNoLimit ||
         s.type_filter[0] != '\0';
}

}  // namespace core
