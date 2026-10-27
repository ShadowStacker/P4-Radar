// Trace files can hold thousands of points of 9-14 values each, which would
// cost megabytes as a cJSON tree. This is a small single-pass scanner instead.

#include "core/trace.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace core::trace {

namespace {

class Scanner {
 public:
  Scanner(const char* s, size_t n) : p_(s), end_(s + n) {}

  bool done() const { return p_ >= end_; }
  void ws() {
    while (p_ < end_ && (*p_ == ' ' || *p_ == '\n' || *p_ == '\r' || *p_ == '\t')) ++p_;
  }
  bool eat(char c) {
    ws();
    if (p_ < end_ && *p_ == c) {
      ++p_;
      return true;
    }
    return false;
  }
  char peek() {
    ws();
    return p_ < end_ ? *p_ : '\0';
  }

  /** Reads a JSON string into `out` (escapes are kept as-is; keys never have any). */
  bool string(std::string* out) {
    if (!eat('"')) return false;
    const char* start = p_;
    while (p_ < end_ && *p_ != '"') {
      if (*p_ == '\\') ++p_;
      ++p_;
    }
    if (p_ >= end_) return false;
    if (out) out->assign(start, p_ - start);
    ++p_;
    return true;
  }

  bool number(double* out) {
    ws();
    char buf[40];
    size_t n = 0;
    while (p_ < end_ && n < sizeof(buf) - 1 &&
           (std::strchr("+-.eE", *p_) != nullptr || (*p_ >= '0' && *p_ <= '9'))) {
      buf[n++] = *p_++;
    }
    if (n == 0) return false;
    buf[n] = '\0';
    *out = std::strtod(buf, nullptr);
    return true;
  }

  bool literal(const char* word) {
    ws();
    const size_t n = std::strlen(word);
    if (static_cast<size_t>(end_ - p_) < n || std::strncmp(p_, word, n) != 0) return false;
    p_ += n;
    return true;
  }

  /** Skips any JSON value. */
  bool skip() {
    const char c = peek();
    if (c == '"') return string(nullptr);
    if (c == '{' || c == '[') {
      const char close = c == '{' ? '}' : ']';
      ++p_;
      if (eat(close)) return true;
      for (;;) {
        if (c == '{') {
          if (!string(nullptr) || !eat(':')) return false;
        }
        if (!skip()) return false;
        if (eat(',')) continue;
        return eat(close);
      }
    }
    if (literal("null") || literal("true") || literal("false")) return true;
    double d;
    return number(&d);
  }

 private:
  const char* p_;
  const char* end_;
};

struct RawPoint {
  Point pt;
  bool new_leg;
};

/** One trace entry: [t, lat, lon, alt|"ground"|null, gs, track, flags, ...]. */
bool parsePoint(Scanner& sc, RawPoint* out, bool* valid) {
  if (!sc.eat('[')) return false;
  *valid = true;
  out->pt = {};
  out->new_leg = false;
  int idx = 0;
  if (sc.eat(']')) {
    *valid = false;
    return true;
  }
  for (;;) {
    double v = 0;
    bool have = false;
    const char c = sc.peek();
    if (idx <= 6 && (c == '-' || (c >= '0' && c <= '9'))) {
      if (!sc.number(&v)) return false;
      have = true;
    } else if (idx == 3 && c == '"') {
      std::string s;
      if (!sc.string(&s)) return false;
      if (s == "ground") out->pt.flags |= kGround;
    } else if (!sc.skip()) {
      return false;
    }
    switch (idx) {
      case 0: if (have) out->pt.t = static_cast<float>(v); else *valid = false; break;
      case 1: if (have) out->pt.lat = static_cast<float>(v); else *valid = false; break;
      case 2: if (have) out->pt.lon = static_cast<float>(v); else *valid = false; break;
      case 3: if (have) out->pt.alt_ft = static_cast<int32_t>(v); break;
      case 6: if (have && (static_cast<int>(v) & 2)) out->new_leg = true; break;
      default: break;
    }
    ++idx;
    if (sc.eat(',')) continue;
    if (!sc.eat(']')) return false;
    break;
  }
  if (*valid && (std::fabs(out->pt.lat) > 90 || std::fabs(out->pt.lon) > 180)) *valid = false;
  return true;
}

}  // namespace

std::vector<std::string> hostsFor(DataSource src) {
  static const char* const kHosts[] = {"globe.adsb.fi", "globe.airplanes.live", "adsb.lol"};
  int first = 0;
  switch (src) {
    case DataSource::AirplanesLive: first = 1; break;
    case DataSource::AdsbLol: first = 2; break;
    case DataSource::AdsbFi:
    default: first = 0; break;
  }
  std::vector<std::string> out;
  out.emplace_back(kHosts[first]);
  for (int i = 0; i < 3; ++i) {
    if (i != first) out.emplace_back(kHosts[i]);
  }
  return out;
}

std::string buildUrl(const std::string& host, const char* hex, bool full) {
  const size_t n = std::strlen(hex);
  const std::string last2 = n >= 2 ? std::string(hex + n - 2) : std::string(hex);
  return "https://" + host + "/data/traces/" + last2 + (full ? "/trace_full_" : "/trace_recent_") + hex +
         ".json";
}

bool parse(const char* json, size_t len, Trace* out) {
  out->points.clear();
  out->timestamp = 0;
  Scanner sc(json, len);
  if (!sc.eat('{')) return false;

  std::vector<RawPoint> raw;
  bool have_trace = false;
  if (!sc.eat('}')) {
    for (;;) {
      std::string key;
      if (!sc.string(&key) || !sc.eat(':')) return false;
      if (key == "timestamp") {
        if (!sc.number(&out->timestamp)) return false;
      } else if (key == "trace") {
        if (!sc.eat('[')) return false;
        have_trace = true;
        if (!sc.eat(']')) {
          for (;;) {
            RawPoint rp;
            bool valid;
            if (!parsePoint(sc, &rp, &valid)) return false;
            if (valid) {
              raw.push_back(rp);
            } else if (rp.new_leg && !raw.empty()) {
              raw.back().new_leg = true;  // keep the leg marker even without a position
            }
            if (sc.eat(',')) continue;
            if (!sc.eat(']')) return false;
            break;
          }
        }
      } else if (!sc.skip()) {
        return false;
      }
      if (sc.eat(',')) continue;
      if (!sc.eat('}')) return false;
      break;
    }
  }
  if (!have_trace || raw.empty()) return false;

  // Current flight: from the last "new leg" marker, and no more than kMaxHours.
  size_t start = 0;
  for (size_t i = raw.size(); i-- > 0;) {
    if (raw[i].new_leg) {
      start = i;
      break;
    }
  }
  const float t_end = raw.back().pt.t;
  while (start < raw.size() && t_end - raw[start].pt.t > kMaxHours * 3600.0f) ++start;

  const size_t n = raw.size() - start;
  const size_t step = (n + kMaxPoints - 1) / kMaxPoints;  // >= 1
  out->points.reserve(n / step + 2);
  for (size_t i = start; i < raw.size(); ++i) {
    if ((i - start) % step != 0 && i != raw.size() - 1) continue;
    Point pt = raw[i].pt;
    if (pt.flags & kGround) pt.alt_ft = 0;
    if (!out->points.empty() && pt.t - out->points.back().t > kGapSeconds) pt.flags |= kGapBefore;
    out->points.push_back(pt);
  }
  return out->points.size() >= 2;
}

}  // namespace core::trace
