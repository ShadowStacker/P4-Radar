#include "core/adsb.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace core::adsb {

namespace {

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

namespace {

// A single-pass JSON reader for feed responses. It validates the whole
// document like cJSON (which this replaces: building a node per value for
// hundreds of aircraft cost far more than reading them) and keeps only the
// fields the radar uses. Behaviour matches cJSON_ParseWithLength() +
// cJSON_GetObjectItemCaseSensitive(): first occurrence of a key wins,
// trailing data after the document is ignored, escapes are decoded.

enum class Kind : uint8_t { None, Number, String, Other };

struct Field {
  Kind kind = Kind::None;  // None = key not seen
  double num = 0;
  std::string str;
};

// The keys read from each aircraft object.
enum Key : uint8_t {
  kLat, kLon, kAltBaro, kAltGeom, kTrueHeading, kMagHeading, kTrack, kGs, kTas, kIas, kBaroRate, kGeomRate,
  kSeenPos, kHex, kFlight, kR, kT, kSquawk, kCategory, kEmergency, kDbFlags, kKeyCount
};
const char* const kKeyNames[kKeyCount] = {"lat",      "lon",       "alt_baro", "alt_geom", "true_heading", "mag_heading",
                                         "track",    "gs",        "tas",      "ias",      "baro_rate",    "geom_rate",
                                         "seen_pos", "hex",       "flight",   "r",        "t",            "squawk",
                                         "category", "emergency", "dbFlags"};
const uint8_t kKeyLens[kKeyCount] = {3, 3, 8, 8, 12, 11, 5, 2, 3, 3, 9, 9, 8, 3, 6, 1, 1, 6, 8, 9, 7};

bool num(const std::vector<Field>& f, Key k, double* out) {
  if (f[k].kind != Kind::Number) return false;
  *out = f[k].num;
  return true;
}

/** Copies a string field, trimming trailing spaces (callsigns are padded). */
void str(const std::vector<Field>& f, Key k, char* out, size_t out_len) {
  out[0] = '\0';
  if (f[k].kind != Kind::String) return;
  const char* s = f[k].str.c_str();  // (stops at an embedded NUL, like a C string)
  size_t n = strnlen(s, out_len - 1);
  while (n > 0 && s[n - 1] == ' ') --n;
  std::memcpy(out, s, n);
  out[n] = '\0';
}

/** One aircraft object's fields to *a; false if it's skipped (no position, filtered out, no address). */
bool toAircraft(const std::vector<Field>& p, const ParseOptions& opt, Aircraft* out) {
  Aircraft a;
  if (!num(p, kLat, &a.lat) || !num(p, kLon, &a.lon)) return false;

  double v = 0;
  if (p[kAltBaro].kind == Kind::String && std::strcmp(p[kAltBaro].str.c_str(), "ground") == 0) {
    a.on_ground = true;
    a.alt_ft = 0;
    a.alt_valid = true;
  } else if (num(p, kAltBaro, &v) || num(p, kAltGeom, &v)) {
    a.alt_ft = static_cast<int32_t>(std::lround(v));
    a.alt_valid = true;
  }

  if (a.on_ground && !opt.include_ground) return false;
  if (!a.on_ground && a.alt_valid && (a.alt_ft < opt.min_alt_ft || a.alt_ft > opt.max_alt_ft)) return false;

  // Track is the direction of motion (used for dead reckoning); heading is
  // where the nose points (used for the symbol). Each falls back to the other.
  double hdg = 0;
  const bool have_hdg = num(p, kTrueHeading, &hdg) || num(p, kMagHeading, &hdg);
  if (num(p, kTrack, &v)) a.track_deg = static_cast<float>(v);
  else if (have_hdg) a.track_deg = static_cast<float>(hdg);
  a.heading_deg = have_hdg ? static_cast<float>(hdg) : a.track_deg;

  if (num(p, kGs, &v) || num(p, kTas, &v) || num(p, kIas, &v)) a.gs_kt = static_cast<float>(v);
  if (num(p, kBaroRate, &v) || num(p, kGeomRate, &v)) a.vrate_fpm = static_cast<int32_t>(std::lround(v));
  if (num(p, kSeenPos, &v)) a.seen_pos_s = static_cast<float>(v < 0 ? 0 : (v > 60 ? 60 : v));

  str(p, kHex, a.hex, sizeof(a.hex));
  str(p, kFlight, a.callsign, sizeof(a.callsign));
  str(p, kR, a.reg, sizeof(a.reg));
  str(p, kT, a.type, sizeof(a.type));
  str(p, kSquawk, a.squawk, sizeof(a.squawk));
  str(p, kCategory, a.category, sizeof(a.category));

  char emergency[16];
  str(p, kEmergency, emergency, sizeof(emergency));
  a.emergency = isEmergencySquawk(a.squawk) || (emergency[0] != '\0' && std::strcmp(emergency, "none") != 0);

  // dbFlags: 1 = military, 2 = interesting, 4 = PIA, 8 = LADD (readsb convention).
  if (num(p, kDbFlags, &v)) a.military = (static_cast<int>(v) & 1) != 0;
  if (opt.military_only && !a.military) return false;

  if (a.hex[0] == '\0') return false;  // can't track it across fetches
  *out = a;
  return true;
}

class Reader {
 public:
  Reader(const char* s, size_t n, const ParseOptions& opt) : p_(s), end_(s + n), opt_(opt), fields_(kKeyCount) {}

  /** The whole document; the aircraft of the first "ac" and "aircraft" arrays go to the two lists. */
  bool document(std::vector<Aircraft>* ac, bool* ac_is_array, std::vector<Aircraft>* aircraft,
                bool* aircraft_is_array, bool* root_is_object) {
    if (end_ - p_ >= 3 && std::memcmp(p_, "\xEF\xBB\xBF", 3) == 0) p_ += 3;  // UTF-8 BOM
    skipWs();
    *root_is_object = p_ < end_ && *p_ == '{';
    if (!*root_is_object) return value(nullptr, 0);
    ++p_;
    skipWs();
    if (p_ < end_ && *p_ == '}') {
      ++p_;
      return true;
    }
    bool seen_ac = false, seen_aircraft = false;
    for (;;) {
      std::string key;
      skipWs();
      if (!string(&key)) return false;
      skipWs();
      if (p_ >= end_ || *p_ != ':') return false;
      ++p_;
      skipWs();
      std::vector<Aircraft>* list = nullptr;
      bool* is_array = nullptr;
      if (!seen_ac && std::strcmp(key.c_str(), "ac") == 0) {
        seen_ac = true;
        list = ac;
        is_array = ac_is_array;
      } else if (!seen_aircraft && std::strcmp(key.c_str(), "aircraft") == 0) {
        seen_aircraft = true;
        list = aircraft;
        is_array = aircraft_is_array;
      }
      if (list != nullptr && p_ < end_ && *p_ == '[') {
        *is_array = true;
        if (!aircraftArray(list)) return false;
      } else if (!value(nullptr, 1)) {
        return false;
      }
      skipWs();
      if (p_ < end_ && *p_ == ',') {
        ++p_;
        continue;
      }
      if (p_ < end_ && *p_ == '}') {
        ++p_;
        return true;
      }
      return false;
    }
  }

 private:
  const char* p_;
  const char* end_;
  const ParseOptions& opt_;
  std::vector<Field> fields_;  // the aircraft object being read
  std::string key_scratch_;

  /** Which wanted key this is (-1: none). */
  static int keyIndex(const char* key, size_t n) {
    // The plain key: one or two letters ("r", "t", "gs") through "true_heading".
    for (int k = 0; k < kKeyCount; ++k) {
      if (kKeyLens[k] == n && key[0] == kKeyNames[k][0] && std::memcmp(key, kKeyNames[k], n) == 0) return k;
    }
    return -1;
  }
  // A feed is 3-4 levels deep. Each level is a stack frame on the network task
  // (16 KB), so garbage nested thousands deep is refused long before that runs out.
  static constexpr int kMaxDepth = 32;

  void skipWs() {
    while (p_ < end_ && static_cast<unsigned char>(*p_) <= 32) ++p_;
  }

  bool literal(const char* word) {
    const size_t n = std::strlen(word);
    if (static_cast<size_t>(end_ - p_) < n || std::memcmp(p_, word, n) != 0) return false;
    p_ += n;
    return true;
  }

  static int hexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  }

  /** Four hex digits; like cJSON, anything that isn't one makes the code 0. */
  static unsigned hex4(const char* at) {
    unsigned v = 0;
    for (int i = 0; i < 4; ++i) {
      const int d = hexDigit(at[i]);
      if (d < 0) return 0;
      v = v * 16 + static_cast<unsigned>(d);
    }
    return v;
  }

  static void putUtf8(unsigned cp, std::string* out) {
    if (cp < 0x80) {
      out->push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
      out->push_back(static_cast<char>(0xC0 | (cp >> 6)));
      out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
      out->push_back(static_cast<char>(0xE0 | (cp >> 12)));
      out->push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
      out->push_back(static_cast<char>(0xF0 | (cp >> 18)));
      out->push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
      out->push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
  }

  /**
   * A string (at '"'); decoded into *out if given. Like cJSON, the end is
   * found first (a backslash hides the next character), then the escapes
   * in between are decoded.
   */
  bool string(std::string* out) {
    const char* s;
    size_t n;
    if (!stringView(&s, &n, out)) return false;
    if (out && s != out->data()) out->assign(s, n);
    return true;
  }

  /**
   * A string as (*s, *n): pointing into the document when it has no escapes,
   * else decoded into *scratch (or a temporary one, just to check it).
   */
  bool stringView(const char** s, size_t* n, std::string* scratch) {
    if (p_ >= end_ || *p_ != '"') return false;
    const char* const start = p_ + 1;
    const char* q = start;
    bool escaped = false;
    while (q < end_ && *q != '"') {
      if (*q == '\\') {
        if (q + 1 >= end_) return false;
        escaped = true;
        ++q;
      }
      ++q;
    }
    if (q >= end_) return false;
    const char* const close = q;
    p_ = close + 1;
    if (!escaped) {
      *s = start;
      *n = static_cast<size_t>(close - start);
      return true;
    }
    std::string temp;
    std::string* o = scratch ? scratch : &temp;  // still decoded: a bad escape fails the document
    o->clear();
    for (const char* c = start; c < close;) {
      if (*c != '\\') {
        o->push_back(*c++);
        continue;
      }
      switch (c[1]) {
        case 'b': o->push_back('\b'); break;
        case 'f': o->push_back('\f'); break;
        case 'n': o->push_back('\n'); break;
        case 'r': o->push_back('\r'); break;
        case 't': o->push_back('\t'); break;
        case '"':
        case '\\':
        case '/': o->push_back(c[1]); break;
        case 'u': {
          if (close - c < 6) return false;
          unsigned cp = hex4(c + 2);
          if (cp >= 0xDC00 && cp <= 0xDFFF) return false;  // a lone low surrogate
          if (cp >= 0xD800 && cp <= 0xDBFF) {
            const char* lo_at = c + 6;
            if (close - lo_at < 6 || lo_at[0] != '\\' || lo_at[1] != 'u') return false;
            const unsigned lo = hex4(lo_at + 2);
            if (lo < 0xDC00 || lo > 0xDFFF) return false;
            cp = 0x10000 + (((cp & 0x3FF) << 10) | (lo & 0x3FF));
            c += 6;
          }
          putUtf8(cp, o);
          c += 6;
          continue;
        }
        default: return false;
      }
      c += 2;
    }
    *s = o->data();
    *n = std::strlen(o->c_str());  // a decoded \u0000 ends it, as in cJSON's C strings
    return true;
  }

  /**
   * A number. cJSON hands the run of "0123456789+-eE." to strtod, so this
   * accepts exactly what strtod reads from it (and strtod runs only for the
   * numbers that are kept).
   */
  bool number(double* out) {
    const char* q = p_;
    if (q < end_ && (*q == '-' || *q == '+')) ++q;
    int digits = 0;
    while (q < end_ && *q >= '0' && *q <= '9') ++q, ++digits;
    if (q < end_ && *q == '.') {
      ++q;
      while (q < end_ && *q >= '0' && *q <= '9') ++q, ++digits;
    }
    if (digits == 0) return false;
    if (q < end_ && (*q == 'e' || *q == 'E')) {
      const char* e = q + 1;
      if (e < end_ && (*e == '-' || *e == '+')) ++e;
      if (e < end_ && *e >= '0' && *e <= '9') {
        while (e < end_ && *e >= '0' && *e <= '9') ++e;
        q = e;
      }
    }
    if (out && fastDecimal(p_, q, out)) {
      // done
    } else if (out) {
      const size_t n = static_cast<size_t>(q - p_);
      char small[64];
      std::string big;
      char* buf = small;
      if (n >= sizeof(small)) {
        big.assign(p_, n);
        buf = &big[0];
      } else {
        std::memcpy(small, p_, n);
        small[n] = '\0';
      }
      *out = std::strtod(buf, nullptr);
    }
    p_ = q;
    return true;
  }

  /**
   * [s, e) as a double without strtod when that's exact: at most 15 digits
   * and no exponent. Both the digits and the power of ten are then exact
   * doubles, so one correctly rounded division gives strtod's own answer.
   */
  static bool fastDecimal(const char* s, const char* e, double* out) {
    static const double kPow10[] = {1, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11, 1e12, 1e13, 1e14, 1e15};
    bool neg = false;
    if (*s == '-' || *s == '+') neg = *s++ == '-';
    uint64_t m = 0;
    int digits = 0, frac = -1;
    for (; s < e; ++s) {
      if (*s == '.') {
        frac = 0;
        continue;
      }
      if (*s < '0' || *s > '9') return false;  // an exponent
      if (++digits > 15) return false;
      m = m * 10 + static_cast<uint64_t>(*s - '0');
      if (frac >= 0) ++frac;
    }
    double v = static_cast<double>(m);
    if (frac > 0) v /= kPow10[frac];
    *out = neg ? -v : v;
    return true;
  }

  /** Any value; *f (if given) records its kind and, for numbers and strings, its value. */
  bool value(Field* f, int depth) {
    if (p_ >= end_) return false;
    if (depth > kMaxDepth) return false;
    const char c = *p_;
    if (c == '"') {
      if (f) f->kind = Kind::String;
      return string(f ? &f->str : nullptr);
    }
    if (c == '-' || (c >= '0' && c <= '9')) {
      if (f) f->kind = Kind::Number;
      return number(f ? &f->num : nullptr);
    }
    if (f) f->kind = Kind::Other;
    if (c == 'n') return literal("null");
    if (c == 't') return literal("true");
    if (c == 'f') return literal("false");
    if (c == '[') return array(depth + 1);
    if (c == '{') return object(nullptr, depth + 1);
    return false;
  }

  bool array(int depth) {
    ++p_;  // '['
    skipWs();
    if (p_ < end_ && *p_ == ']') {
      ++p_;
      return true;
    }
    for (;;) {
      skipWs();
      if (!value(nullptr, depth)) return false;
      skipWs();
      if (p_ < end_ && *p_ == ',') {
        ++p_;
        continue;
      }
      if (p_ < end_ && *p_ == ']') {
        ++p_;
        return true;
      }
      return false;
    }
  }

  /** An object; with `fields`, the first value of each wanted key is kept. */
  bool object(std::vector<Field>* fields, int depth) {
    ++p_;  // '{'
    skipWs();
    if (p_ < end_ && *p_ == '}') {
      ++p_;
      return true;
    }
    for (;;) {
      skipWs();
      const char* key;
      size_t key_len;
      if (!stringView(&key, &key_len, &key_scratch_)) return false;
      skipWs();
      if (p_ >= end_ || *p_ != ':') return false;
      ++p_;
      skipWs();
      Field* f = nullptr;
      if (fields != nullptr) {
        const int k = keyIndex(key, key_len);
        if (k >= 0 && (*fields)[k].kind == Kind::None) f = &(*fields)[k];
      }
      if (!value(f, depth)) return false;
      skipWs();
      if (p_ < end_ && *p_ == ',') {
        ++p_;
        continue;
      }
      if (p_ < end_ && *p_ == '}') {
        ++p_;
        return true;
      }
      return false;
    }
  }

  /** The aircraft array: objects are read field by field, anything else skipped. */
  bool aircraftArray(std::vector<Aircraft>* out) {
    ++p_;  // '['
    skipWs();
    if (p_ < end_ && *p_ == ']') {
      ++p_;
      return true;
    }
    for (;;) {
      skipWs();
      if (p_ < end_ && *p_ == '{') {
        for (Field& f : fields_) f.kind = Kind::None;
        if (!object(&fields_, 2)) return false;
        Aircraft a;
        if (toAircraft(fields_, opt_, &a)) out->push_back(a);
      } else if (!value(nullptr, 2)) {
        return false;
      }
      skipWs();
      if (p_ < end_ && *p_ == ',') {
        ++p_;
        continue;
      }
      if (p_ < end_ && *p_ == ']') {
        ++p_;
        return true;
      }
      return false;
    }
  }
};

}  // namespace

bool parse(const char* json, size_t len, const ParseOptions& opt, std::vector<Aircraft>* out) {
  out->clear();
  std::vector<Aircraft> aircraft;  // only if the feed calls the list that
  bool ac_is_array = false, aircraft_is_array = false, root_is_object = false;
  Reader reader(json, len, opt);
  if (!reader.document(out, &ac_is_array, &aircraft, &aircraft_is_array, &root_is_object)) {
    out->clear();
    return false;
  }
  if (!ac_is_array && !aircraft_is_array) return root_is_object;  // no traffic: may omit the array or send null
  if (!ac_is_array) out->swap(aircraft);
  return true;
}

size_t dropRadarDuplicates(std::vector<Aircraft>* list) {
  constexpr double kSameKm = 2.0;
  constexpr int32_t kSameFt = 500;
  constexpr double kKmPerKtS = kKmPerNm / 3600.0;
  // Where each one is now: its position moved on by its age at its speed.
  auto now = [](const Aircraft& a) {
    const LatLon p{a.lat, a.lon};
    if (a.on_ground || a.gs_kt < 1.0f || a.seen_pos_s <= 0) return p;
    return destination(p, a.track_deg, a.gs_kt * kKmPerKtS * a.seen_pos_s);
  };
  std::vector<LatLon> real;
  std::vector<const Aircraft*> real_ac;
  bool any_radar = false;
  for (const Aircraft& a : *list) {
    if (a.hex[0] == '~') {
      any_radar = true;
    } else {
      real.push_back(now(a));
      real_ac.push_back(&a);
    }
  }
  if (!any_radar || real.empty()) return 0;
  std::vector<bool> drop(list->size(), false);
  size_t n = 0;
  for (size_t i = 0; i < list->size(); ++i) {
    const Aircraft& a = (*list)[i];
    if (a.hex[0] != '~') continue;
    const LatLon p = now(a);
    for (size_t j = 0; j < real.size(); ++j) {
      const Aircraft& b = *real_ac[j];
      if (a.alt_valid && b.alt_valid && std::abs(a.alt_ft - b.alt_ft) > kSameFt) continue;
      if (a.on_ground != b.on_ground) continue;
      if (distanceKm(p, real[j]) <= kSameKm) {
        drop[i] = true;
        ++n;
        break;
      }
    }
  }
  if (n == 0) return 0;
  size_t w = 0;
  for (size_t i = 0; i < list->size(); ++i) {
    if (!drop[i]) {
      if (w != i) (*list)[w] = (*list)[i];
      ++w;
    }
  }
  list->resize(w);
  return n;
}

}  // namespace core::adsb
