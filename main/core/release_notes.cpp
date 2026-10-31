#include "core/release_notes.h"

#include <cstring>

#include "core/update.h"

namespace core::release_notes {

namespace {

std::string trim(const std::string& s) {
  size_t a = 0, b = s.size();
  while (a < b && (s[a] == ' ' || s[a] == '\t')) ++a;
  while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
  return s.substr(a, b - a);
}

/** True if the '.' at `i` ends "vs.", "e.g." or "i.e." rather than a sentence. */
bool abbreviation(const std::string& s, size_t i) {
  for (const char* a : {"vs", "e.g", "i.e", "etc"}) {
    const size_t n = std::strlen(a);
    if (i >= n && s.compare(i - n, n, a) == 0 && (i == n || s[i - n - 1] == ' ' || s[i - n - 1] == '(')) return true;
  }
  return false;
}

/** "Title. First sentence." from a bullet's text. */
std::string brief(const std::string& bullet) {
  std::string title, rest = bullet;
  if (rest.compare(0, 2, "**") == 0) {
    const size_t end = rest.find("**", 2);
    if (end != std::string::npos) {
      title = rest.substr(2, end - 2);
      rest = trim(rest.substr(end + 2));
    }
  }
  // The first sentence: up to ". " (not inside parentheses), or all of it.
  int depth = 0;
  size_t stop = std::string::npos;
  for (size_t i = 0; i + 1 < rest.size(); ++i) {
    if (rest[i] == '(') ++depth;
    else if (rest[i] == ')' && depth > 0) --depth;
    else if (rest[i] == '.' && rest[i + 1] == ' ' && depth == 0 && !abbreviation(rest, i)) {
      stop = i + 1;
      break;
    }
  }
  std::string first = stop == std::string::npos ? rest : rest.substr(0, stop);
  if (first.size() > 160) {
    size_t cut = first.rfind(' ', 157);
    first = first.substr(0, cut == std::string::npos ? 157 : cut) + "...";
  }
  std::string out = title;
  if (!out.empty() && !first.empty()) {
    const char last = out.back();
    if (last != '.' && last != ':' && last != '!' && last != '?' && first[0] != '(') out += ":";
    out += " ";
  }
  if (!first.empty() && (first[0] == ':' || first[0] == ',')) first = trim(first.substr(1));
  return plain(out + first);
}

}  // namespace

std::string plain(const std::string& md) {
  static const struct {
    const char* from;
    const char* to;
  } kSwap[] = {{"\xE2\x86\x92", ">"},  {"\xE2\x80\x93", "-"},  {"\xE2\x80\x94", " - "}, {"\xE2\x80\xA6", "..."},
               {"\xE2\x80\x99", "'"},  {"\xE2\x80\x98", "'"},  {"\xE2\x80\x9C", "\""},  {"\xE2\x80\x9D", "\""},
               {"\xC3\x97", "x"},      {"\xE2\x89\x88", "~"}};
  std::string out;
  for (size_t i = 0; i < md.size();) {
    bool swapped = false;
    for (const auto& s : kSwap) {
      const size_t n = std::strlen(s.from);
      if (md.compare(i, n, s.from) == 0) {
        out += s.to;
        i += n;
        swapped = true;
        break;
      }
    }
    if (swapped) continue;
    const unsigned char c = static_cast<unsigned char>(md[i]);
    if (md.compare(i, 2, "**") == 0) {
      i += 2;
    } else if (c == '`') {
      ++i;
    } else if (c == '[') {
      // [text](link) -> text
      const size_t close = md.find("](", i);
      const size_t end = close == std::string::npos ? std::string::npos : md.find(')', close);
      if (end != std::string::npos) {
        out += md.substr(i + 1, close - i - 1);
        i = end + 1;
      } else {
        out.push_back(md[i++]);
      }
    } else if (c == 0xC2 && i + 1 < md.size() && static_cast<unsigned char>(md[i + 1]) == 0xB0) {
      out += "\xC2\xB0";  // degree sign: the fonts have it
      i += 2;
    } else if (c >= 0x80) {
      // Anything else outside ASCII: skip the whole UTF-8 character.
      ++i;
      while (i < md.size() && (static_cast<unsigned char>(md[i]) & 0xC0) == 0x80) ++i;
    } else {
      out.push_back(md[i++]);
    }
  }
  return out;
}

std::vector<Section> parse(const std::string& md) {
  std::vector<Section> out;
  std::string bullet;
  auto flush = [&] {
    if (!bullet.empty() && !out.empty()) out.back().items.push_back(brief(trim(bullet)));
    bullet.clear();
  };
  size_t start = 0;
  while (start < md.size()) {
    size_t end = md.find('\n', start);
    if (end == std::string::npos) end = md.size();
    const std::string line = md.substr(start, end - start);
    start = end + 1;
    if (line.compare(0, 4, "## [") == 0) {
      flush();
      const size_t close = line.find(']');
      Section s;
      s.version = close == std::string::npos ? "" : line.substr(4, close - 4);
      const size_t dash = line.find(" - ", close == std::string::npos ? 0 : close);
      if (dash != std::string::npos) s.date = trim(line.substr(dash + 3));
      out.push_back(s);
    } else if (line.compare(0, 2, "- ") == 0 || line.compare(0, 4, "  - ") == 0) {
      flush();  // a bullet (sub-bullets become items of their own)
      bullet = trim(line).substr(2);
    } else if (!bullet.empty() && line.compare(0, 2, "  ") == 0 && !trim(line).empty()) {
      bullet += " " + trim(line);  // the bullet continues
    } else {
      flush();
    }
  }
  flush();
  return out;
}

std::vector<Section> newSince(const std::vector<Section>& all, const std::string& current, const std::string& previous,
                              int max) {
  std::vector<Section> out;
  for (const Section& s : all) {
    if (static_cast<int>(out.size()) >= max) break;
    if (update::compareVersions(s.version.c_str(), current.c_str()) > 0) continue;  // newer than what runs
    if (previous.empty()) {
      if (update::compareVersions(s.version.c_str(), current.c_str()) == 0) out.push_back(s);
      continue;
    }
    if (update::compareVersions(s.version.c_str(), previous.c_str()) <= 0) break;
    out.push_back(s);
  }
  return out;
}

}  // namespace core::release_notes
