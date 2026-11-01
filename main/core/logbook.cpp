#include "core/logbook.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace core::logbook {

namespace {

constexpr size_t kMergeAt = 256;

// Days since 1970-01-01 for a civil date (and back): Howard Hinnant's algorithms.
int64_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

int civilFromDays(int64_t z) {
  z += 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int64_t y = static_cast<int64_t>(yoe) + era * 400;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  const unsigned d = doy - (153 * mp + 2) / 5 + 1;
  const unsigned m = mp + (mp < 10 ? 3 : -9);
  return static_cast<int>((y + (m <= 2)) * 10000 + m * 100 + d);
}

void copyField(char* out, size_t n, const char* s) {
  size_t i = 0;
  for (; s[i] != '\0' && i + 1 < n; ++i) out[i] = (s[i] == ',' || s[i] == '\n' || s[i] == '"') ? ' ' : s[i];
  out[i] = '\0';
}

}  // namespace

const char* csvHeader() { return "date,time,hex,callsign,registration,type,visit,unix_time\n"; }

uint32_t parseHex(const char* s) {
  uint32_t v = 0;
  int n = 0;
  for (; s[n] != '\0'; ++n) {
    const char c = s[n];
    int d;
    if (c >= '0' && c <= '9') d = c - '0';
    else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
    else return 0;
    v = v * 16 + static_cast<uint32_t>(d);
  }
  return n == 6 ? v : 0;
}

int addDays(int date, int days) {
  return civilFromDays(daysFromCivil(date / 10000, static_cast<unsigned>(date / 100 % 100),
                                     static_cast<unsigned>(date % 100)) + days);
}

const Entry* Book::find(uint32_t hex) const { return const_cast<Book*>(this)->findMutable(hex); }

Entry* Book::findMutable(uint32_t hex) {
  auto it = std::lower_bound(main_.begin(), main_.end(), hex, [](const Entry& e, uint32_t h) { return e.hex < h; });
  if (it != main_.end() && it->hex == hex) return &*it;
  for (Entry& e : recent_) {
    if (e.hex == hex) return &e;
  }
  return nullptr;
}

void Book::merge() {
  if (recent_.empty()) return;
  std::sort(recent_.begin(), recent_.end(), [](const Entry& a, const Entry& b) { return a.hex < b.hex; });
  const size_t mid = main_.size();
  main_.insert(main_.end(), recent_.begin(), recent_.end());
  std::inplace_merge(main_.begin(), main_.begin() + static_cast<std::ptrdiff_t>(mid), main_.end(),
                     [](const Entry& a, const Entry& b) { return a.hex < b.hex; });
  recent_.clear();
}

std::vector<Entry> Book::observe(const std::vector<Aircraft>& near_home, int64_t unix_s, int date, int time_hhmm,
                                 std::string* csv) {
  std::vector<Entry> firsts;
  if (start_date_ == 0) start_date_ = date;
  for (const Aircraft& a : near_home) {
    const uint32_t hex = parseHex(a.hex);
    if (hex == 0) continue;
    const char* label = a.callsign[0] ? a.callsign : (a.reg[0] ? a.reg : a.hex);
    Entry* e = findMutable(hex);
    bool new_visit = false, first = false;
    if (e == nullptr) {
      Entry n;
      n.hex = hex;
      n.first_date = static_cast<uint32_t>(date);
      n.first_s = unix_s;
      recent_.push_back(n);
      e = &recent_.back();
      new_visit = first = true;
    } else if (unix_s - e->last_s >= kVisitGapS) {
      new_visit = true;
    }
    e->last_s = unix_s;
    copyField(e->label, sizeof(e->label), label);
    if (a.type[0]) copyField(e->type, sizeof(e->type), a.type);
    if (!new_visit) continue;
    ++e->visits;
    ++visits_;
    if (first) firsts.push_back(*e);
    if (csv != nullptr) {
      char reg[12], cs[12], type[5];
      copyField(cs, sizeof(cs), a.callsign);
      copyField(reg, sizeof(reg), a.reg);
      copyField(type, sizeof(type), a.type);
      char line[128];
      std::snprintf(line, sizeof(line), "%04d-%02d-%02d,%02d:%02d,%06x,%s,%s,%s,%s,%lld\n", date / 10000,
                    date / 100 % 100, date % 100, time_hhmm / 100, time_hhmm % 100, static_cast<unsigned>(hex), cs,
                    reg, type, first ? "first" : "again", static_cast<long long>(unix_s));
      *csv += line;
    }
    if (recent_.size() >= kMergeAt) merge();
  }
  return firsts;
}

void Book::loadLine(const std::string& line) {
  // date,time,hex,callsign,registration,type,visit,unix_time
  const char* f[8];
  std::string s = line;
  if (!s.empty() && s.back() == '\r') s.pop_back();
  int n = 0;
  char* p = &s[0];
  f[n++] = p;
  for (; *p != '\0' && n < 8; ++p) {
    if (*p == ',') {
      *p = '\0';
      f[n++] = p + 1;
    }
  }
  if (n < 8) return;
  int y, m, d;
  if (std::sscanf(f[0], "%d-%d-%d", &y, &m, &d) != 3) return;
  const uint32_t hex = parseHex(f[2]);
  if (hex == 0) return;
  const int date = y * 10000 + m * 100 + d;
  const int64_t t = std::strtoll(f[7], nullptr, 10);
  if (start_date_ == 0 || date < start_date_) start_date_ = date;
  Entry* e = findMutable(hex);
  if (e == nullptr) {
    Entry ne;
    ne.hex = hex;
    ne.first_date = static_cast<uint32_t>(date);
    ne.first_s = t;
    recent_.push_back(ne);
    e = &recent_.back();
  }
  if (t >= e->last_s) {
    e->last_s = t;
    const char* label = f[3][0] ? f[3] : (f[4][0] ? f[4] : f[2]);
    copyField(e->label, sizeof(e->label), label);
    if (f[5][0]) copyField(e->type, sizeof(e->type), f[5]);
  }
  ++e->visits;
  ++visits_;
  if (recent_.size() >= kMergeAt * 16) merge();
}

void Book::loaded() { merge(); }

bool Book::learning(int today) const {
  return start_date_ == 0 || today < addDays(start_date_, kLearningDays);
}

namespace {

/** The `n` best entries by `better`, best first, without copying the whole book. */
template <typename Better>
std::vector<Entry> top(const std::vector<Entry>& a, const std::vector<Entry>& b, size_t n, Better better) {
  std::vector<const Entry*> all;
  all.reserve(a.size() + b.size());
  for (const Entry& e : a) all.push_back(&e);
  for (const Entry& e : b) all.push_back(&e);
  n = std::min(n, all.size());
  std::partial_sort(all.begin(), all.begin() + static_cast<std::ptrdiff_t>(n), all.end(),
                    [&](const Entry* x, const Entry* y) { return better(*x, *y); });
  std::vector<Entry> out;
  for (size_t i = 0; i < n; ++i) out.push_back(*all[i]);
  return out;
}

}  // namespace

std::vector<Entry> Book::frequent(size_t n) const {
  return top(main_, recent_, n, [](const Entry& a, const Entry& b) {
    return a.visits != b.visits ? a.visits > b.visits : a.last_s > b.last_s;
  });
}

std::vector<Entry> Book::newest(size_t n) const {
  return top(main_, recent_, n, [](const Entry& a, const Entry& b) { return a.first_s > b.first_s; });
}

void Book::clear() {
  main_.clear();
  recent_.clear();
  visits_ = 0;
  start_date_ = 0;
}

}  // namespace core::logbook
