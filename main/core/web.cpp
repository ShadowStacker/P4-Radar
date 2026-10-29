#include "core/web.h"

#include <cstdio>

namespace core::web {

namespace {
int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
}  // namespace

std::string urlDecode(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '+') {
      out.push_back(' ');
    } else if (s[i] == '%' && i + 2 < s.size() && hexVal(s[i + 1]) >= 0 && hexVal(s[i + 2]) >= 0) {
      out.push_back(static_cast<char>(hexVal(s[i + 1]) * 16 + hexVal(s[i + 2])));
      i += 2;
    } else {
      out.push_back(s[i]);
    }
  }
  return out;
}

std::map<std::string, std::string> parseForm(const std::string& s) {
  std::map<std::string, std::string> out;
  size_t start = 0;
  while (start <= s.size()) {
    size_t end = s.find('&', start);
    if (end == std::string::npos) end = s.size();
    const std::string part = s.substr(start, end - start);
    if (!part.empty()) {
      const size_t eq = part.find('=');
      out[urlDecode(part.substr(0, eq))] = eq == std::string::npos ? "" : urlDecode(part.substr(eq + 1));
    }
    start = end + 1;
  }
  return out;
}

std::string cookie(const std::string& header, const char* name) {
  const std::string key = std::string(name) + "=";
  size_t pos = 0;
  while (pos < header.size()) {
    while (pos < header.size() && (header[pos] == ' ' || header[pos] == ';')) ++pos;
    size_t end = header.find(';', pos);
    if (end == std::string::npos) end = header.size();
    if (header.compare(pos, key.size(), key) == 0) return header.substr(pos + key.size(), end - pos - key.size());
    pos = end;
  }
  return "";
}

std::string jsonEscape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (unsigned char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out.push_back(static_cast<char>(c));
        }
    }
  }
  return out;
}

std::string jsonString(const std::string& s) { return "\"" + jsonEscape(s) + "\""; }

int LoginGuard::lockedFor(uint32_t now_ms) const {
  if (fails_ < kMaxTries) return 0;
  const int32_t left = static_cast<int32_t>(locked_until_ - now_ms);
  return left > 0 ? (left + 999) / 1000 : 0;
}

void LoginGuard::failed(uint32_t now_ms) {
  if (fails_ >= kMaxTries && lockedFor(now_ms) == 0) fails_ = kMaxTries - 1;  // the lock ran out: one more try
  if (++fails_ >= kMaxTries) locked_until_ = now_ms + kLockMs;
}

}  // namespace core::web
