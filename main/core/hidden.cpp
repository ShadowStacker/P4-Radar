#include "core/hidden.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace core::hidden {

namespace {

uint32_t hex6(const char* s) {
  uint32_t v = 0;
  for (int i = 0; i < 6; ++i) {
    const char c = s[i];
    int d;
    if (c >= '0' && c <= '9') d = c - '0';
    else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
    else return 0xFFFFFFFFu;
    v = v * 16 + static_cast<uint32_t>(d);
  }
  return s[6] == '\0' ? v : 0xFFFFFFFFu;
}

bool byKey(const Entry& e, uint32_t key) { return e.key < key; }

}  // namespace

uint32_t keyOf(const char* hex) {
  if (hex == nullptr) return 0;
  const bool non_icao = hex[0] == '~';
  const uint32_t v = hex6(non_icao ? hex + 1 : hex);
  if (v == 0xFFFFFFFFu) return 0;
  // Bit 25 marks "a valid code", so 000000 isn't 0.
  return v | (non_icao ? 1u << 24 : 0u) | (1u << 25);
}

bool List::contains(uint32_t key) const {
  if (key == 0) return false;
  auto it = std::lower_bound(entries_.begin(), entries_.end(), key, byKey);
  return it != entries_.end() && it->key == key;
}

bool List::add(const Aircraft& a) {
  const uint32_t key = keyOf(a.hex);
  if (key == 0) return false;
  auto it = std::lower_bound(entries_.begin(), entries_.end(), key, byKey);
  if (it != entries_.end() && it->key == key) return false;
  Entry e;
  e.key = key;
  std::snprintf(e.hex, sizeof(e.hex), "%s", a.hex);
  for (char* c = e.hex; *c; ++c) *c = static_cast<char>(std::tolower(static_cast<unsigned char>(*c)));
  const char* name = a.callsign[0] ? a.callsign : a.reg;
  std::snprintf(e.label, sizeof(e.label), "%s", name);
  // Callsigns come padded with spaces.
  for (size_t n = std::strlen(e.label); n > 0 && e.label[n - 1] == ' '; --n) e.label[n - 1] = '\0';
  std::snprintf(e.type, sizeof(e.type), "%s", a.type);
  entries_.insert(it, e);
  return true;
}

bool List::remove(uint32_t key) {
  auto it = std::lower_bound(entries_.begin(), entries_.end(), key, byKey);
  if (it == entries_.end() || it->key != key) return false;
  entries_.erase(it);
  return true;
}

void List::filter(std::vector<Aircraft>* aircraft) const {
  if (entries_.empty()) return;
  aircraft->erase(std::remove_if(aircraft->begin(), aircraft->end(),
                                 [this](const Aircraft& a) { return contains(keyOf(a.hex)); }),
                  aircraft->end());
}

std::string List::serialize() const {
  std::string out =
      "# Hidden aircraft: never shown and never a sound. One per line: hex code, then\n"
      "# (optional) the name and type it was hidden with. Delete a line to bring it back.\n";
  for (const Entry& e : entries_) {
    out += e.hex;
    if (e.label[0] || e.type[0]) {
      out += ' ';
      out += e.label[0] ? e.label : "-";
    }
    if (e.type[0]) {
      out += ' ';
      out += e.type;
    }
    out += '\n';
  }
  return out;
}

void List::parse(const std::string& text) {
  entries_.clear();
  size_t pos = 0;
  while (pos < text.size()) {
    size_t end = text.find('\n', pos);
    if (end == std::string::npos) end = text.size();
    std::string line = text.substr(pos, end - pos);
    pos = end + 1;
    const size_t hash = line.find('#');
    if (hash != std::string::npos) line.resize(hash);
    std::vector<std::string> words;
    size_t i = 0;
    while (i < line.size()) {
      while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
      size_t j = i;
      while (j < line.size() && !std::isspace(static_cast<unsigned char>(line[j]))) ++j;
      if (j > i) words.push_back(line.substr(i, j - i));
      i = j;
    }
    if (words.empty()) continue;
    Aircraft a;
    std::snprintf(a.hex, sizeof(a.hex), "%s", words[0].c_str());
    if (words[0].size() >= sizeof(a.hex) || keyOf(a.hex) == 0) continue;
    if (words.size() > 1 && words[1] != "-") std::snprintf(a.callsign, sizeof(a.callsign), "%s", words[1].c_str());
    if (words.size() > 1 && words[1] != "-" && words[1].size() >= sizeof(a.callsign)) {
      a.callsign[0] = '\0';
      std::snprintf(a.reg, sizeof(a.reg), "%s", words[1].c_str());
    }
    if (words.size() > 2) std::snprintf(a.type, sizeof(a.type), "%s", words[2].c_str());
    add(a);
  }
}

}  // namespace core::hidden
