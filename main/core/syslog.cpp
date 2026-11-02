#include "core/syslog.h"

#include <mutex>

namespace core::syslog {

namespace {

std::mutex s_mu;
std::string s_recent;   // whole lines
std::string s_pending;  // text not handed to the card yet
std::string s_partial;  // the line being printed (no '\n' yet)
uint64_t s_total = 0;
size_t s_dropped = 0;   // pending bytes lost since the last takePending()
bool s_in_code = false; // inside an escape sequence split across calls
void (*s_sink)(const char*, size_t) = nullptr;

/** Keeps at most `cap` bytes of `s`, cutting at a line start. */
void trimFront(std::string* s, size_t cap, size_t* dropped) {
  if (s->size() <= cap) return;
  size_t cut = s->size() - cap;
  const size_t nl = s->find('\n', cut);
  cut = nl == std::string::npos ? s->size() : nl + 1;
  if (dropped) *dropped += cut;
  s->erase(0, cut);
}

}  // namespace

void append(const char* text, size_t len, const char* stamp) {
  if (len == 0) return;
  std::lock_guard<std::mutex> lock(s_mu);
  s_total += len;
  for (size_t i = 0; i < len; ++i) {
    const char c = text[i];
    if (s_in_code) {
      if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) s_in_code = false;
      continue;
    }
    if (c == '\x1b') {
      s_in_code = true;
      continue;
    }
    if (c == '\r') continue;
    if (s_partial.empty() && stamp != nullptr) {
      if (c == '\n') continue;  // empty lines aren't worth a time stamp
      s_partial = stamp;
    }
    s_partial.push_back(c);
    if (c == '\n' || s_partial.size() >= 1024) {
      if (c != '\n') s_partial.push_back('\n');
      if (s_sink) s_sink(s_partial.data(), s_partial.size());
      s_recent += s_partial;
      s_pending += s_partial;
      s_partial.clear();
    }
  }
  // Trimmed in steps, not on every line (fewer big moves in PSRAM).
  if (s_recent.size() > kRecentBytes + 4096) trimFront(&s_recent, kRecentBytes, nullptr);
  if (s_pending.size() > kPendingBytes + 4096) trimFront(&s_pending, kPendingBytes, &s_dropped);
}

void setLineSink(void (*sink)(const char* line, size_t len)) {
  std::lock_guard<std::mutex> lock(s_mu);
  s_sink = sink;
}

std::string recent() {
  std::lock_guard<std::mutex> lock(s_mu);
  if (s_recent.size() <= kRecentBytes) return s_recent;
  const size_t nl = s_recent.find('\n', s_recent.size() - kRecentBytes);
  return nl == std::string::npos ? std::string() : s_recent.substr(nl + 1);
}

std::string takePending() {
  std::lock_guard<std::mutex> lock(s_mu);
  std::string out;
  if (s_dropped > 0) {
    out = "(" + std::to_string(s_dropped) + " bytes of log lost: the card wasn't keeping up)\n";
    s_dropped = 0;
  }
  out += s_pending;
  s_pending.clear();
  return out;
}

uint64_t totalBytes() {
  std::lock_guard<std::mutex> lock(s_mu);
  return s_total;
}

void reset() {
  std::lock_guard<std::mutex> lock(s_mu);
  s_recent.clear();
  s_pending.clear();
  s_partial.clear();
  s_total = 0;
  s_dropped = 0;
  s_in_code = false;
}

}  // namespace core::syslog
