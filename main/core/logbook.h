#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/traffic.h"

/**
 * The logbook: every aircraft ever seen near home, how often it came by
 * (visits) and when first and last. Kept on the SD card as logbook.csv, one
 * line per visit, so it opens in a spreadsheet; the radar rebuilds its index
 * from the file at startup.
 */
namespace core::logbook {

constexpr int64_t kVisitGapS = 30 * 60;  // seen again after this long away: a new visit
constexpr int kLearningDays = 2;         // no "first time here" alerts until the book has run this long

struct Entry {
  uint32_t hex = 0;         // ICAO address
  uint32_t first_date = 0;  // yyyymmdd (local)
  int64_t first_s = 0;      // unix time of the first sighting
  int64_t last_s = 0;       // unix time last seen
  uint32_t visits = 0;
  char label[12] = "";      // callsign or registration, as last seen
  char type[5] = "";
};

/** The CSV header line (with '\n'). */
const char* csvHeader();

/** 6 hex digits to a number; 0 if it isn't one (e.g. "~" non-ICAO addresses). */
uint32_t parseHex(const char* s);

/** yyyymmdd plus `days` (may be negative). */
int addDays(int date, int days);

class Book {
 public:
  /**
   * Adds the aircraft near home from one fetch. Appends a CSV line to *csv for
   * each new visit, and returns the aircraft seen for the very first time.
   * `date`/`time_hhmm` are local, for the CSV.
   */
  std::vector<Entry> observe(const std::vector<Aircraft>& near_home, int64_t unix_s, int date, int time_hhmm,
                             std::string* csv);

  /**
   * Rebuilds the index from logbook.csv, a line at a time (lines that don't
   * parse are skipped). The lines are only collected here; loaded() sorts
   * them once and folds them into the index (fast for a book of years).
   */
  void loadLine(const std::string& line) { loadLine(line.data(), line.size()); }
  void loadLine(const char* line, size_t len);
  /** Call after the last loadLine(). */
  void loaded();

  /** First day in the book (0 = empty). */
  int startDate() const { return start_date_; }
  /** True while the book is too new for "first time here" to mean much. */
  bool learning(int today) const;

  size_t aircraft() const { return main_.size() + recent_.size(); }
  uint64_t visits() const { return visits_; }
  const Entry* find(uint32_t hex) const;
  /** The `n` aircraft with the most visits (most first). */
  std::vector<Entry> frequent(size_t n) const;
  /** The `n` newest first-time aircraft (newest first). */
  std::vector<Entry> newest(size_t n) const;

  void clear();

 private:
  Entry* findMutable(uint32_t hex);
  void merge();

  std::vector<Entry> main_;    // sorted by hex (one big block: PSRAM on the device)
  std::vector<Entry> recent_;  // added since the last merge, unsorted (small)
  struct Line {                // a parsed logbook.csv line, until loaded()
    uint32_t hex;
    uint32_t date;
    int64_t t;
    uint32_t order;            // position in the file (ties keep the later line)
    char label[12];
    char type[5];
  };
  std::vector<Line> pending_;
  uint64_t visits_ = 0;
  int start_date_ = 0;
};

}  // namespace core::logbook
