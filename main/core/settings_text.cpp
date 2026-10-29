#include "core/settings_text.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/clock.h"
#include "core/filter.h"

namespace core::settings_text {

namespace {

enum class Kind : uint8_t {
  Section,  // heading line; key is the title
  Bool,
  U8,
  U16,
  I32,
  F32,
  F64,
  Str,       // char[size]
  Enum,      // uint8_t index into names
  Rotation,  // uint8_t quarter turns, written in degrees
  Categories,
  Alarm,     // alarms[min]
};

struct Field {
  const char* key;
  Kind kind;
  size_t offset;
  size_t size;  // Str: buffer size
  double min, max;
  const char* const* names;  // Enum
  int name_count;
  const char* help;          // comment line above the value, or nullptr
};

const char* const kUnits[] = {"km", "mi", "nm"};
const char* const kThemes[] = {"phosphor", "amber", "ocean", "night", "custom"};
const char* const kLabels[] = {"off", "callsign", "full"};
const char* const kColorModes[] = {"theme", "altitude"};
const char* const kSources[] = {"adsb.fi", "airplanes.live", "adsb.lol"};
const char* const kLabelSizes[] = {"small", "normal", "large"};
const char* const kCodeStyles[] = {"icao", "iata"};
const char* const kMaps[] = {"off", "streets", "topo", "faa"};
const char* const kCategories[] = {"light", "small", "airliner", "heavy", "fast", "rotor", "other", "unknown"};
const char* const kDays[] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};

#define OFF(f) offsetof(Settings, f)
#define SECTION(t) {t, Kind::Section, 0, 0, 0, 0, nullptr, 0, nullptr}
#define BOOL(f, h) {#f, Kind::Bool, OFF(f), 0, 0, 1, nullptr, 0, h}
#define U8(f, lo, hi, h) {#f, Kind::U8, OFF(f), 0, lo, hi, nullptr, 0, h}
#define U16(f, lo, hi, h) {#f, Kind::U16, OFF(f), 0, lo, hi, nullptr, 0, h}
#define I32(f, lo, hi, h) {#f, Kind::I32, OFF(f), 0, lo, hi, nullptr, 0, h}
#define F32(f, lo, hi, h) {#f, Kind::F32, OFF(f), 0, lo, hi, nullptr, 0, h}
#define F64(f, lo, hi, h) {#f, Kind::F64, OFF(f), 0, lo, hi, nullptr, 0, h}
#define STR(f, h) {#f, Kind::Str, OFF(f), sizeof(Settings::f), 0, 0, nullptr, 0, h}
#define ENUM(f, n, h) {#f, Kind::Enum, OFF(f), 0, 0, 0, n, static_cast<int>(sizeof(n) / sizeof(n[0])), h}
#define ALARM(i) {"alarm" #i, Kind::Alarm, 0, 0, (i) - 1, 0, nullptr, 0, nullptr}

static_assert(kMaxSavedWifi == 4, "one SAVED_WIFI() line per saved network below");

const Field kFields[] = {
    SECTION("Wi-Fi"),
    STR(wifi_ssid, "The network to use first: name and password"),
    STR(wifi_pass, nullptr),
#define SAVED_WIFI(n)                                                                                    \
  {"wifi" #n "_ssid", Kind::Str, OFF(wifi_saved[(n) - 2].ssid), sizeof(SavedWifi::ssid), 0, 0, nullptr, 0, \
   (n) == 2 ? "More networks, tried in turn when the first can't connect (empty = unused)" : nullptr},   \
  {"wifi" #n "_pass", Kind::Str, OFF(wifi_saved[(n) - 2].pass), sizeof(SavedWifi::pass), 0, 0, nullptr, 0, nullptr}
    SAVED_WIFI(2),
    SAVED_WIFI(3),
    SAVED_WIFI(4),
    SAVED_WIFI(5),
#undef SAVED_WIFI

    SECTION("Location"),
    F64(home_lat, -90, 90, "Home position in decimal degrees (north and east are positive)"),
    F64(home_lon, -180, 180, nullptr),
    F32(home_radius_km, 2, 400, "Radius the Home button zooms to (always in km)"),
    BOOL(home_set, "off = ask for a location at startup"),

    SECTION("Display"),
    ENUM(units, kUnits, "km, mi or nm"),
    ENUM(theme, kThemes, "phosphor, amber, ocean, night or custom"),
    U8(brightness, 5, 100, "Percent"),
    {"rotation", Kind::Rotation, OFF(rotation), 0, 0, 3, nullptr, 0,
     "Screen rotation, clockwise: 0, 90, 180 or 270 (takes a restart)"},
    ENUM(labels, kLabels, "Aircraft labels: off, callsign or full"),
    ENUM(color_mode, kColorModes, "Aircraft colors: theme or altitude"),
    U8(aircraft_scale, 60, 160, "Aircraft symbol size, percent"),
    ENUM(label_size, kLabelSizes, "small, normal or large"),
    BOOL(show_trails, nullptr),
    BOOL(extended_trail, "Download the flight history of the tapped aircraft"),
    BOOL(show_speed_vectors, nullptr),
    BOOL(show_rings, nullptr),
    BOOL(show_runways, nullptr),
    BOOL(show_airport_labels, nullptr),
    ENUM(airport_codes, kCodeStyles, "icao or iata"),
    BOOL(show_route_info, "Look up the airline and route of the tapped aircraft"),
    BOOL(autohide_buttons, "Fade the radar buttons after 10 seconds"),
    U8(dim_after_min, 0, 255, "Dim after this many idle minutes (0 = never)"),
    U8(dim_brightness, 0, 50, "Percent while dimmed"),

    SECTION("Sweep"),
    BOOL(sweep_enabled, nullptr),
    U8(sweep_rpm, 4, 60, "Revolutions per minute"),
    U8(sweep_trail_deg, 0, 180, "Length of the fading trail, degrees"),
    BOOL(sweep_smooth, nullptr),
    BOOL(sweep_reveal, "Aircraft fade until the beam passes them"),

    SECTION("Custom theme"),
    U16(custom_hue, 0, 359, "Hues are 0-359, saturation and brightness percent"),
    U8(custom_sat, 0, 100, nullptr),
    U8(custom_bg, 0, 30, "Background brightness"),
    U8(custom_grid, 15, 100, "Rings and compass brightness"),
    U16(custom_aircraft_hue, 0, 359, nullptr),
    U8(custom_aircraft_sat, 0, 100, "0 = white"),
    U16(custom_label_hue, 0, 359, nullptr),
    U8(custom_label_sat, 0, 100, "0 = white"),
    U16(custom_alt_low_hue, 0, 359, "Altitude colors: ground, 10,000 ft and 40,000 ft"),
    U16(custom_alt_mid_hue, 0, 359, nullptr),
    U16(custom_alt_high_hue, 0, 359, nullptr),

    SECTION("Background map"),
    ENUM(map_style, kMaps, "off, streets, topo or faa"),
    BOOL(map_labels, "Place names on the street map"),
    BOOL(map_tint, "Recolor the map in the theme color"),
    U8(map_brightness, 10, 100, "Percent"),
    BOOL(sd_cache, "Keep downloaded map tiles on the SD card"),
    STR(carto_key, "CARTO API key for the dark street map (empty = OpenStreetMap)"),

    SECTION("Sound"),
    BOOL(sound_enabled, nullptr),
    U8(volume, 0, 100, "Percent"),
    BOOL(sound_touch, nullptr),
    BOOL(sound_new_aircraft, nullptr),
    BOOL(sound_sweep_ping, nullptr),
    BOOL(sound_alerts, "Emergency squawks"),
    BOOL(sound_proximity, "Any aircraft inside the alert zone"),
    F32(alert_radius_km, 0.5, 50, "Alert zone around home: radius in km, below this altitude in ft"),
    I32(alert_max_alt_ft, 500, 20000, nullptr),
    BOOL(sound_favorite_seen, nullptr),
    BOOL(sound_favorite_near, nullptr),
    F32(favorite_near_radius_km, 1, 200, "Favorite nearby radius, km"),

    SECTION("Filters"),
    BOOL(show_ground, nullptr),
    BOOL(military_only, nullptr),
    I32(min_alt_ft, 0, 40000, "Altitude range, ft"),
    I32(max_alt_ft, 1000, 60000, nullptr),
    U16(min_speed_kt, 0, 600, "Speed range, knots (1000 = no limit)"),
    U16(max_speed_kt, 50, 1000, nullptr),
    {"categories", Kind::Categories, OFF(category_mask), 0, 0, 0, nullptr, 0,
     "Kinds to show: all, or a list of light, small, airliner, heavy, fast, rotor, other, unknown"},
    STR(type_filter, "Only these types, e.g. \"B738, A32*\" (empty = all)"),

    SECTION("Data"),
    ENUM(source, kSources, "adsb.fi, airplanes.live or adsb.lol"),
    U8(poll_interval_s, 2, 30, "Seconds between updates"),

    SECTION("Clock"),
    BOOL(clock_mode, "Clock face instead of compass headings"),
    BOOL(clock_24h, nullptr),
    BOOL(clock_seconds, nullptr),
    BOOL(clock_smooth_seconds, nullptr),
    BOOL(clock_show_date, nullptr),
    U16(clock_hand_hue, 0, 359, "Hand colors: hue 0-359, saturation percent (0 = white)"),
    U8(clock_hand_sat, 0, 100, nullptr),
    U16(clock_second_hue, 0, 359, nullptr),
    U8(clock_second_sat, 0, 100, nullptr),
    U8(time_zone, 0, 255, nullptr),  // help is the zone list (see serialize)
    BOOL(wake_on_alarm, "Alarms ring while the screen sleeps (else only a tap wakes it)"),
    ALARM(1),
    ALARM(2),
    ALARM(3),
    ALARM(4),
};

#undef OFF
#undef SECTION
#undef BOOL
#undef U8
#undef U16
#undef I32
#undef F32
#undef F64
#undef STR
#undef ENUM
#undef ALARM

template <typename T>
T& at(Settings* s, size_t offset) {
  return *reinterpret_cast<T*>(reinterpret_cast<char*>(s) + offset);
}
template <typename T>
const T& at(const Settings& s, size_t offset) {
  return *reinterpret_cast<const T*>(reinterpret_cast<const char*>(&s) + offset);
}

std::string lower(std::string v) {
  for (char& c : v) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return v;
}

std::string trim(const std::string& v) {
  size_t a = 0, b = v.size();
  while (a < b && std::isspace(static_cast<unsigned char>(v[a]))) ++a;
  while (b > a && std::isspace(static_cast<unsigned char>(v[b - 1]))) --b;
  return v.substr(a, b - a);
}

/** Splits on commas and white space. */
std::vector<std::string> words(const std::string& v) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : v) {
    if (c == ',' || std::isspace(static_cast<unsigned char>(c))) {
      if (!cur.empty()) out.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  if (!cur.empty()) out.push_back(cur);
  return out;
}

std::string formatNumber(double v) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.6f", v);
  // Trim trailing zeros: 25.000000 -> 25, 44.254100 -> 44.2541.
  std::string out = buf;
  if (out.find('.') != std::string::npos) {
    while (out.back() == '0') out.pop_back();
    if (out.back() == '.') out.pop_back();
  }
  return out;
}

std::string formatDaysText(uint8_t days) {
  days &= 0x7F;
  if (days == 0) return "once";
  if (days == 0x7F) return "daily";
  if (days == 0x3E) return "weekdays";
  if (days == 0x41) return "weekends";
  std::string out;
  for (int d = 0; d < 7; ++d) {
    if (!(days & (1 << d))) continue;
    if (!out.empty()) out += ",";
    out += kDays[d];
  }
  return out;
}

int dayIndex(const std::string& w) {
  for (int d = 0; d < 7; ++d) {
    if (w.compare(0, 3, kDays[d]) == 0 && w.size() >= 3) return d;
  }
  return -1;
}

std::string formatValue(const Settings& s, const Field& f) {
  switch (f.kind) {
    case Kind::Section: return "";
    case Kind::Bool: return at<bool>(s, f.offset) ? "on" : "off";
    case Kind::U8: return std::to_string(at<uint8_t>(s, f.offset));
    case Kind::U16: return std::to_string(at<uint16_t>(s, f.offset));
    case Kind::I32: return std::to_string(at<int32_t>(s, f.offset));
    case Kind::F32: return formatNumber(at<float>(s, f.offset));
    case Kind::F64: return formatNumber(at<double>(s, f.offset));
    case Kind::Str: return std::string("\"") + (reinterpret_cast<const char*>(&s) + f.offset) + "\"";
    case Kind::Enum: {
      const uint8_t v = at<uint8_t>(s, f.offset);
      return v < f.name_count ? f.names[v] : f.names[0];
    }
    case Kind::Rotation: return std::to_string((at<uint8_t>(s, f.offset) & 3) * 90);
    case Kind::Categories: {
      const uint8_t m = at<uint8_t>(s, f.offset);
      if (m == 0xFF) return "all";
      std::string out;
      for (int i = 0; i < kCategoryCount; ++i) {
        if (!(m & (1 << i))) continue;
        if (!out.empty()) out += ", ";
        out += kCategories[i];
      }
      return out.empty() ? "none" : out;
    }
    case Kind::Alarm: {
      const Alarm& a = s.alarms[static_cast<int>(f.min)];
      char buf[64];
      std::snprintf(buf, sizeof(buf), "%s %02d:%02d %s", a.enabled ? "on" : "off", a.hour, a.minute,
                    formatDaysText(a.days).c_str());
      return buf;
    }
  }
  return "";
}

bool parseBool(const std::string& v, bool* out) {
  const std::string l = lower(v);
  if (l == "on" || l == "true" || l == "yes" || l == "1") *out = true;
  else if (l == "off" || l == "false" || l == "no" || l == "0") *out = false;
  else return false;
  return true;
}

bool parseDouble(const std::string& v, double* out) {
  if (v.empty()) return false;
  char* end = nullptr;
  const double d = std::strtod(v.c_str(), &end);
  if (end == v.c_str() || *end != '\0' || !std::isfinite(d)) return false;
  *out = d;
  return true;
}

bool parseAlarm(const std::string& v, Alarm* a) {
  std::vector<std::string> w = words(lower(v));
  if (w.size() < 2) return false;
  Alarm r = *a;
  if (!parseBool(w[0], &r.enabled)) return false;
  int h = -1, m = -1;
  char tail = 0;
  if (std::sscanf(w[1].c_str(), "%d:%d%c", &h, &m, &tail) != 2 || h < 0 || h > 23 || m < 0 || m > 59) return false;
  r.hour = static_cast<uint8_t>(h);
  r.minute = static_cast<uint8_t>(m);
  if (w.size() > 2) {
    uint8_t days = 0;
    for (size_t i = 2; i < w.size(); ++i) {
      const std::string& d = w[i];
      if (d == "once" || d == "never") continue;
      if (d == "daily" || d == "everyday" || d == "all") { days |= 0x7F; continue; }
      if (d == "weekdays") { days |= 0x3E; continue; }
      if (d == "weekends") { days |= 0x41; continue; }
      const size_t dash = d.find('-');
      if (dash != std::string::npos) {  // range like mon-fri
        const int from = dayIndex(d.substr(0, dash)), to = dayIndex(d.substr(dash + 1));
        if (from < 0 || to < 0) return false;
        for (int k = from;; k = (k + 1) % 7) {
          days |= static_cast<uint8_t>(1 << k);
          if (k == to) break;
        }
        continue;
      }
      const int k = dayIndex(d);
      if (k < 0) return false;
      days |= static_cast<uint8_t>(1 << k);
    }
    r.days = days;
  }
  *a = r;
  return true;
}

bool parseValue(const Field& f, const std::string& raw, Settings* s) {
  std::string v = raw;
  switch (f.kind) {
    case Kind::Section: return false;
    case Kind::Bool: return parseBool(v, &at<bool>(s, f.offset));
    case Kind::U8:
    case Kind::U16:
    case Kind::I32:
    case Kind::F32:
    case Kind::F64: {
      double d;
      if (!parseDouble(v, &d)) return false;
      d = std::min(std::max(d, f.min), f.max);
      if (f.kind == Kind::U8) at<uint8_t>(s, f.offset) = static_cast<uint8_t>(std::lround(d));
      else if (f.kind == Kind::U16) at<uint16_t>(s, f.offset) = static_cast<uint16_t>(std::lround(d));
      else if (f.kind == Kind::I32) at<int32_t>(s, f.offset) = static_cast<int32_t>(std::lround(d));
      else if (f.kind == Kind::F32) at<float>(s, f.offset) = static_cast<float>(d);
      else at<double>(s, f.offset) = d;
      return true;
    }
    case Kind::Str: {
      if (v.size() >= 2 && v.front() == '"' && v.back() == '"') v = v.substr(1, v.size() - 2);
      if (v.size() >= f.size) return false;  // too long: keep the old value rather than cut it
      char* dst = reinterpret_cast<char*>(s) + f.offset;
      std::memcpy(dst, v.c_str(), v.size() + 1);
      return true;
    }
    case Kind::Enum: {
      const std::string l = lower(v);
      for (int i = 0; i < f.name_count; ++i) {
        if (l == f.names[i]) {
          at<uint8_t>(s, f.offset) = static_cast<uint8_t>(i);
          return true;
        }
      }
      // Without the dot also works ("adsbfi", "airplaneslive").
      for (int i = 0; i < f.name_count; ++i) {
        std::string n = f.names[i];
        n.erase(std::remove(n.begin(), n.end(), '.'), n.end());
        if (l == n) {
          at<uint8_t>(s, f.offset) = static_cast<uint8_t>(i);
          return true;
        }
      }
      return false;
    }
    case Kind::Rotation: {
      double d;
      if (!parseDouble(v, &d)) return false;
      const long deg = std::lround(d);
      if (deg % 90 != 0) return false;
      at<uint8_t>(s, f.offset) = static_cast<uint8_t>(((deg / 90) % 4 + 4) % 4);
      return true;
    }
    case Kind::Categories: {
      const std::vector<std::string> w = words(lower(v));
      uint8_t m = 0;
      for (const std::string& c : w) {
        if (c == "all") { m = 0xFF; continue; }
        if (c == "none") continue;
        int found = -1;
        for (int i = 0; i < kCategoryCount; ++i) {
          if (c == kCategories[i]) found = i;
        }
        if (found < 0) return false;
        m |= static_cast<uint8_t>(1 << found);
      }
      at<uint8_t>(s, f.offset) = m;
      return true;
    }
    case Kind::Alarm: return parseAlarm(v, &s->alarms[static_cast<int>(f.min)]);
  }
  return false;
}

const Field* findField(const std::string& key) {
  const std::string l = lower(key);
  for (const Field& f : kFields) {
    if (f.kind != Kind::Section && l == f.key) return &f;
  }
  return nullptr;
}

/** Calls fn(line) for each line without its line ending. */
template <typename Fn>
void eachLine(const std::string& text, Fn fn) {
  size_t start = 0;
  while (start <= text.size()) {
    size_t end = text.find('\n', start);
    if (end == std::string::npos) end = text.size();
    std::string line = text.substr(start, end - start);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    fn(line);
    start = end + 1;
  }
}

}  // namespace

std::string serialize(const Settings& s) {
  std::string out =
      "# Plane Radar P4 settings\n"
      "#\n"
      "# The radar reads this file when it starts (or when the card is mounted from\n"
      "# Settings > SD card) and rewrites it whenever you change something on the\n"
      "# screen. Edit it on a computer to change settings; lines can be removed\n"
      "# (they keep their current value) and on/off also accepts true/false.\n"
      "# Lines starting with # are comments.\n";
  for (const Field& f : kFields) {
    if (f.kind == Kind::Section) {
      out += "\n[";
      out += f.key;
      out += "]\n";
      continue;
    }
    if (f.offset == offsetof(Settings, time_zone) && f.kind == Kind::U8) {
      out += "# Time zone number:\n";
      for (int i = 0; i < timeZoneCount(); ++i) {
        char buf[80];
        std::snprintf(buf, sizeof(buf), "#  %2d = %s\n", i, timeZone(i).name);
        out += buf;
      }
    } else if (f.kind == Kind::Alarm && f.min == 0) {
      out += "# Alarms: on/off, 24-hour time, then once, daily, weekdays, weekends or\n"
             "# days like mon,wed,fri or mon-fri\n";
    } else if (f.help) {
      out += "# ";
      out += f.help;
      out += "\n";
    }
    out += f.key;
    out += " = ";
    out += formatValue(s, f);
    out += "\n";
  }
  return out;
}

int parse(const std::string& text, Settings* s, std::vector<std::string>* problems) {
  int count = 0;
  int line_no = 0;
  eachLine(text, [&](const std::string& raw) {
    ++line_no;
    std::string line = trim(raw);
    if (line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[') return;
    const size_t eq = line.find('=');
    auto problem = [&](const std::string& what) {
      if (problems) problems->push_back("line " + std::to_string(line_no) + ": " + what);
    };
    if (eq == std::string::npos) {
      problem("no '=' in \"" + line + "\"");
      return;
    }
    const std::string key = trim(line.substr(0, eq));
    const std::string value = trim(line.substr(eq + 1));
    const Field* f = findField(key);
    if (f == nullptr) {
      problem("unknown setting \"" + key + "\"");
      return;
    }
    if (!parseValue(*f, value, s)) {
      problem("bad value for " + key + ": \"" + value + "\"");
      return;
    }
    ++count;
  });
  // Keep the pairs consistent (the menus never produce min > max).
  if (s->min_alt_ft > s->max_alt_ft) std::swap(s->min_alt_ft, s->max_alt_ft);
  if (s->min_speed_kt > s->max_speed_kt) std::swap(s->min_speed_kt, s->max_speed_kt);
  return count;
}

bool isIcaoHex(const char* s) {
  for (int i = 0; i < 6; ++i) {
    if (!std::isxdigit(static_cast<unsigned char>(s[i]))) return false;
  }
  return s[6] == '\0';
}

std::string serializeFavorites(const std::vector<Favorite>& favs, const std::vector<std::string>& unresolved) {
  std::string out =
      "# Plane Radar P4 favorites\n"
      "#\n"
      "# One aircraft per line: ICAO hex code, then optionally a label and type.\n"
      "# You can also write just a tail number (e.g. N123AB); the radar looks it\n"
      "# up online and fills in the hex code. Up to 32 aircraft. Lines starting\n"
      "# with # are comments. The radar reads this file when it starts and\n"
      "# rewrites it whenever favorites change on the screen.\n"
      "#\n"
      "# hex    label        type\n";
  for (const Favorite& f : favs) {
    char line[64];
    std::snprintf(line, sizeof(line), "%-8s %-12s %s", f.hex, f.label, f.type);
    std::string l = line;
    while (!l.empty() && l.back() == ' ') l.pop_back();
    out += l;
    out += "\n";
  }
  for (const std::string& tail : unresolved) out += tail + "   # not looked up yet\n";
  return out;
}

void parseFavorites(const std::string& text, std::vector<Favorite>* favs, std::vector<std::string>* lookups) {
  favs->clear();
  lookups->clear();
  eachLine(text, [&](const std::string& raw) {
    std::string line = raw;
    const size_t hash = line.find('#');
    if (hash != std::string::npos) line.resize(hash);
    const std::vector<std::string> w = words(line);
    if (w.empty()) return;
    std::string first = w[0];
    for (char& c : first) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (isIcaoHex(first.c_str())) {
      if (containsHex(*favs, lower(first).c_str()) || favs->size() + lookups->size() >= kMaxFavorites) return;
      Favorite f;
      std::snprintf(f.hex, sizeof(f.hex), "%s", lower(first).c_str());
      if (w.size() > 1) std::snprintf(f.label, sizeof(f.label), "%s", w[1].c_str());
      if (w.size() > 2) {
        std::string t = w[2];
        for (char& c : t) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        std::snprintf(f.type, sizeof(f.type), "%s", t.c_str());
      }
      if (f.label[0] == '\0') std::snprintf(f.label, sizeof(f.label), "%s", first.c_str());
      favs->push_back(f);
    } else {
      if (std::find(lookups->begin(), lookups->end(), first) != lookups->end()) return;
      if (favs->size() + lookups->size() >= kMaxFavorites) return;
      lookups->push_back(first);
    }
  });
}

}  // namespace core::settings_text
