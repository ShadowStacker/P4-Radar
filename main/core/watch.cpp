#include "core/watch.h"

#include "core/filter.h"

namespace core {

bool watchActive(const Settings& s) {
  return s.watch_enabled && (s.watch_military || s.watch_rotor || s.watch_fast || s.watch_types[0] != '\0' ||
                             s.watch_callsigns[0] != '\0');
}

std::string watchReason(const Aircraft& a, const Settings& s) {
  if (!s.watch_enabled || a.on_ground) return "";
  if (s.watch_callsigns[0] != '\0') {
    if (a.callsign[0] != '\0' && typeMatches(a.callsign, s.watch_callsigns)) return a.callsign;
    if (a.reg[0] != '\0' && typeMatches(a.reg, s.watch_callsigns)) return a.reg;
  }
  if (s.watch_types[0] != '\0' && a.type[0] != '\0' && typeMatches(a.type, s.watch_types)) return a.type;
  if (s.watch_military && a.military) return "military";
  const CategoryBit cat = categoryOf(a.category);
  if (s.watch_rotor && cat == kCatRotor) return "helicopter";
  if (s.watch_fast && cat == kCatFast) return "fast jet";
  return "";
}

}  // namespace core
