#include "core/backup.h"

#include <cstring>

namespace core::backup {

namespace {

constexpr char kMagic[] = "# Plane Radar backup";

void section(std::string* out, const char* name, const std::string& body) {
  *out += "\n[[";
  *out += name;
  *out += "]]\n";
  *out += body;
  if (!body.empty() && body.back() != '\n') *out += '\n';
}

}  // namespace

std::string make(const std::string& title, const Parts& parts) {
  std::string out = kMagic;
  if (!title.empty()) out += ", " + title;
  out +=
      "\n# Restore it from the System tab of the radar's phone page. It holds your\n"
      "# Wi-Fi passwords: keep it somewhere private.\n";
  section(&out, "settings", parts.settings);
  section(&out, "favorites", parts.favorites);
  section(&out, "silenced", parts.silenced);
  return out;
}

bool split(const std::string& text, Parts* out) {
  *out = Parts();
  size_t start = text.compare(0, 3, "\xEF\xBB\xBF") == 0 ? 3 : 0;
  if (text.compare(start, std::strlen(kMagic), kMagic) != 0) return false;
  std::string* cur = nullptr;
  while (start < text.size()) {
    size_t end = text.find('\n', start);
    if (end == std::string::npos) end = text.size();
    std::string line = text.substr(start, end - start);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    start = end + 1;
    if (line == "[[settings]]") {
      cur = &out->settings;
      out->has_settings = true;
    } else if (line == "[[favorites]]") {
      cur = &out->favorites;
      out->has_favorites = true;
    } else if (line == "[[silenced]]") {
      cur = &out->silenced;
      out->has_silenced = true;
    } else if (line.compare(0, 2, "[[") == 0 && line.size() > 4 && line.compare(line.size() - 2, 2, "]]") == 0) {
      cur = nullptr;  // a section from a newer version
    } else if (cur != nullptr) {
      *cur += line;
      *cur += '\n';
    }
  }
  return out->has_settings || out->has_favorites || out->has_silenced;
}

}  // namespace core::backup
