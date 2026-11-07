#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/traffic.h"

/**
 * Hidden aircraft: never shown, never counted, never a sound (not even an
 * emergency). No limit on how many; kept on the SD card as hidden.txt, one
 * aircraft per line: "a1b2c3 N123AB C172" (hex, then the name and type it
 * was hidden with, for the list).
 */
namespace core::hidden {

struct Entry {
  uint32_t key = 0;     // from the hex code (see keyOf)
  char hex[8] = "";     // as in the feed, e.g. "a1b2c3" or "~1234ab"
  char label[12] = "";  // callsign or registration when hidden
  char type[5] = "";
};

/** A number for a hex code, 0 if it isn't one. Non-ICAO addresses ("~" + 6 hex digits) get bit 24 set. */
uint32_t keyOf(const char* hex);

class List {
 public:
  bool contains(uint32_t key) const;
  bool contains(const char* hex) const { return contains(keyOf(hex)); }
  /** Adds an aircraft. False if it has no usable hex code or is already hidden. */
  bool add(const Aircraft& a);
  /** False if it wasn't hidden. */
  bool remove(uint32_t key);
  /** Takes the hidden aircraft out of a feed update. */
  void filter(std::vector<Aircraft>* aircraft) const;

  size_t size() const { return entries_.size(); }
  const std::vector<Entry>& entries() const { return entries_; }  // sorted by key

  /** hidden.txt. */
  std::string serialize() const;
  /** Reads hidden.txt; lines that don't parse are skipped ("#" starts a comment). */
  void parse(const std::string& text);
  void clear() { entries_.clear(); }

 private:
  std::vector<Entry> entries_;  // sorted by key, unique
};

}  // namespace core::hidden
