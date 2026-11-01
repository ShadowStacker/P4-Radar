#include "core/daylog.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace core::daylog {

namespace {

uint32_t icao(const char* hex) {
  if (hex[0] == '~') return 0;  // TIS-B tracks: no fixed address to count
  char* end = nullptr;
  const unsigned long v = std::strtoul(hex, &end, 16);
  return end != hex && *end == '\0' ? static_cast<uint32_t>(v) : 0;
}

/** Copies a C string into a fixed buffer, cut to fit. */
template <size_t N>
void copyStr(char (&dst)[N], const char* src) {
  const size_t n = strnlen(src, N - 1);
  std::memcpy(dst, src, n);
  dst[n] = '\0';
}

/** Inserts into a sorted vector; true if it was new. */
bool addSorted(std::vector<uint32_t>* v, uint32_t x) {
  auto it = std::lower_bound(v->begin(), v->end(), x);
  if (it != v->end() && *it == x) return false;
  v->insert(it, x);
  return true;
}

void fill(Record* r, const Aircraft& a, float value, int minute) {
  const char* label = a.callsign[0] ? a.callsign : (a.reg[0] ? a.reg : a.hex);
  copyStr(r->label, label);
  copyStr(r->type, a.type);
  r->value = value;
  r->alt_ft = a.alt_ft;
  r->minute = static_cast<int16_t>(minute);
}

std::string hhmm(int minute) {
  const unsigned m = static_cast<unsigned>(minute) % (24 * 60);
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%02u:%02u", m / 60, m % 60);
  return buf;
}

std::string recordLine(const char* key, const Record& r, const char* fmt_value) {
  if (r.minute < 0) return "";
  char value[32];
  std::snprintf(value, sizeof(value), fmt_value, r.value);
  char buf[128];
  std::snprintf(buf, sizeof(buf), "%s = %s %s %s %d ft %s\n", key, r.label, r.type[0] ? r.type : "-", value,
                static_cast<int>(r.alt_ft), hhmm(r.minute).c_str());
  return buf;
}

bool parseRecord(const char* v, Record* r) {
  char label[sizeof(Record::label)], type[sizeof(Record::type)], unit[8];
  float value;
  int alt, h, m;
  if (std::sscanf(v, "%11s %4s %f %7s %d ft %d:%d", label, type, &value, unit, &alt, &h, &m) != 7) return false;
  copyStr(r->label, label);
  copyStr(r->type, std::strcmp(type, "-") == 0 ? "" : type);
  r->value = value;
  r->alt_ft = alt;
  r->minute = static_cast<int16_t>(h * 60 + m);
  return true;
}

const char* const kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

}  // namespace

int dateOf(const std::tm& t) { return (t.tm_year + 1900) * 10000 + (t.tm_mon + 1) * 100 + t.tm_mday; }

bool observe(Day* day, const std::vector<Aircraft>& aircraft, const LatLon& home, double radius_km, int date,
             int minute, bool (*is_favorite)(const char* hex)) {
  bool new_day = false;
  if (day->date != date) {
    *day = Day{};
    day->date = date;
    new_day = true;
  }
  const int hour = (minute / 60) % 24;
  if (hour != day->hour_now) {
    day->hour_now = hour;
    day->hour_seen.clear();
  }
  int now_count = 0;
  for (const Aircraft& a : aircraft) {
    if (a.on_ground) continue;
    const double d = distanceKm(home, {a.lat, a.lon});
    if (d > radius_km) continue;
    ++now_count;
    const uint32_t id = icao(a.hex);
    if (id != 0) addSorted(&day->seen, id);
    if (id != 0 && addSorted(&day->hour_seen, id) && day->hour_seen.size() > day->hourly[hour]) {
      day->hourly[hour] = static_cast<uint16_t>(std::min<size_t>(day->hour_seen.size(), 65535));
    }
    if (id != 0 && a.military) addSorted(&day->military, id);
    if (id != 0 && a.emergency) addSorted(&day->emergencies, id);
    if (is_favorite != nullptr && is_favorite(a.hex)) {
      const char* label = a.callsign[0] ? a.callsign : (a.reg[0] ? a.reg : a.hex);
      if (std::find(day->favorites.begin(), day->favorites.end(), label) == day->favorites.end()) {
        day->favorites.emplace_back(label);
      }
    }
    if (day->closest.minute < 0 || d < day->closest.value) fill(&day->closest, a, static_cast<float>(d), minute);
    if (a.alt_valid && (day->highest.minute < 0 || a.alt_ft > day->highest.value)) {
      fill(&day->highest, a, static_cast<float>(a.alt_ft), minute);
    }
    if (day->fastest.minute < 0 || a.gs_kt > day->fastest.value) fill(&day->fastest, a, a.gs_kt, minute);
  }
  if (now_count > day->peak) {
    day->peak = now_count;
    day->peak_minute = static_cast<int16_t>(minute);
  }
  return new_day;
}

const char* csvHeader() {
  return "date,aircraft,busiest,busiest_at,military,emergencies,h00,h01,h02,h03,h04,h05,h06,h07,h08,h09,h10,h11,"
         "h12,h13,h14,h15,h16,h17,h18,h19,h20,h21,h22,h23\n";
}

std::string csvLine(const Day& d) {
  char buf[96];
  std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d,%d,%d,%s,%d,%d", d.date / 10000, d.date / 100 % 100, d.date % 100,
                d.count(), d.peak, d.peak_minute >= 0 ? hhmm(d.peak_minute).c_str() : "",
                static_cast<int>(d.military.size()), static_cast<int>(d.emergencies.size()));
  std::string out = buf;
  for (int h = 0; h < 24; ++h) out += "," + std::to_string(d.hourly[h]);
  return out + "\n";
}

int busiestHour(const Day& d) {
  int best = -1;
  for (int h = 0; h < 24; ++h) {
    if (d.hourly[h] > 0 && (best < 0 || d.hourly[h] > d.hourly[best])) best = h;
  }
  return best;
}

std::string fileName(int date) {
  char buf[24];
  std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d.txt", date / 10000, date / 100 % 100, date % 100);
  return buf;
}

std::string serialize(const Day& d) {
  char buf[96];
  std::string out = "# Plane Radar P4 daily log: aircraft near home\n";
  std::snprintf(buf, sizeof(buf), "date = %04d-%02d-%02d\n", d.date / 10000, d.date / 100 % 100, d.date % 100);
  out += buf;
  std::snprintf(buf, sizeof(buf), "aircraft = %d\nmilitary = %d\nemergencies = %d\n", d.count(),
                static_cast<int>(d.military.size()), static_cast<int>(d.emergencies.size()));
  out += buf;
  if (d.peak_minute >= 0) out += "busiest = " + std::to_string(d.peak) + " at " + hhmm(d.peak_minute) + "\n";
  out += recordLine("closest", d.closest, "%.2f km");
  out += recordLine("highest", d.highest, "%.0f ft");
  out += recordLine("fastest", d.fastest, "%.0f kt");
  if (!d.favorites.empty()) {
    out += "favorites =";
    for (size_t i = 0; i < d.favorites.size(); ++i) out += (i ? ", " : " ") + d.favorites[i];
    out += "\n";
  }
  auto list = [&](const char* key, const std::vector<uint32_t>& v) {
    out += key;
    out += " =";
    for (uint32_t x : v) {
      std::snprintf(buf, sizeof(buf), " %06x", static_cast<unsigned>(x));
      out += buf;
    }
    out += "\n";
  };
  out += "hourly =";
  for (int h = 0; h < 24; ++h) out += " " + std::to_string(d.hourly[h]);
  out += "\n";
  list("seen", d.seen);
  list("seen_military", d.military);
  list("seen_emergencies", d.emergencies);
  return out;
}

bool parse(const std::string& text, Day* out) {
  *out = Day{};
  size_t pos = 0;
  while (pos < text.size()) {
    size_t end = text.find('\n', pos);
    if (end == std::string::npos) end = text.size();
    const std::string line = text.substr(pos, end - pos);
    pos = end + 1;
    const size_t eq = line.find(" = ");
    if (line.empty() || line[0] == '#' || eq == std::string::npos) continue;
    const std::string key = line.substr(0, eq);
    const char* v = line.c_str() + eq + 3;
    int y, mo, da, n, h, m;
    if (key == "date" && std::sscanf(v, "%d-%d-%d", &y, &mo, &da) == 3) out->date = y * 10000 + mo * 100 + da;
    else if (key == "busiest" && std::sscanf(v, "%d at %d:%d", &n, &h, &m) == 3) {
      out->peak = n;
      out->peak_minute = static_cast<int16_t>(h * 60 + m);
    } else if (key == "closest") parseRecord(v, &out->closest);
    else if (key == "hourly") {
      const char* c = v;
      char* e = nullptr;
      for (int h = 0; h < 24; ++h) {
        const unsigned long x = std::strtoul(c, &e, 10);
        if (e == c) break;
        out->hourly[h] = static_cast<uint16_t>(std::min<unsigned long>(x, 65535));
        c = e;
      }
    }
    else if (key == "highest") parseRecord(v, &out->highest);
    else if (key == "fastest") parseRecord(v, &out->fastest);
    else if (key == "favorites") {
      std::string cur;
      for (const char* c = v;; ++c) {
        if (*c == ',' || *c == '\0') {
          while (!cur.empty() && cur.front() == ' ') cur.erase(cur.begin());
          if (!cur.empty()) out->favorites.push_back(cur);
          cur.clear();
          if (*c == '\0') break;
        } else {
          cur.push_back(*c);
        }
      }
    } else if (key == "seen" || key == "seen_military" || key == "seen_emergencies") {
      std::vector<uint32_t>* dst = key == "seen" ? &out->seen : key == "seen_military" ? &out->military : &out->emergencies;
      const char* c = v;
      char* e = nullptr;
      for (;;) {
        const unsigned long x = std::strtoul(c, &e, 16);
        if (e == c) break;
        dst->push_back(static_cast<uint32_t>(x));
        c = e;
      }
      std::sort(dst->begin(), dst->end());
    }
  }
  return out->date != 0;
}

std::string summary(const Day& d, const char* closest_distance) {
  char buf[128];
  const int month = d.date / 100 % 100;
  std::snprintf(buf, sizeof(buf), "%s %d: %d aircraft", month >= 1 && month <= 12 ? kMonths[month - 1] : "?",
                d.date % 100, d.count());
  std::string out = buf;
  if (d.closest.minute >= 0) out += std::string(", closest ") + d.closest.label + " " + closest_distance;
  return out;
}

}  // namespace core::daylog
