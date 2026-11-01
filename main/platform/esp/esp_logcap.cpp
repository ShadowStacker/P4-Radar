// The system log: printf and ESP_LOG output also go to core::syslog (for the
// phone page and the SD card), and the last lines are kept in RAM that
// survives a crash or watchdog restart.

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <sys/reent.h>

#include "core/syslog.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "platform/platform.h"

namespace plat {

namespace {

constexpr uint32_t kTailMagic = 0x4C4F4731;  // "LOG1"
constexpr size_t kTailBytes = 4096;

// Not cleared at startup: what the previous run printed last.
struct Tail {
  uint32_t magic;
  uint32_t head;  // next write
  uint32_t len;
  char buf[kTailBytes];
};
__NOINIT_ATTR Tail s_tail;

FILE* s_console = nullptr;  // the serial port, as stdout was at startup
std::string s_crash;

void stamp(char* out, size_t n) {
  const time_t t = time(nullptr);
  if (t > 1600000000) {  // the clock is set
    std::tm lt;
    localtime_r(&t, &lt);
    std::strftime(out, n, "%H:%M:%S ", &lt);
  } else {
    std::snprintf(out, n, "+%lus ", static_cast<unsigned long>(esp_log_timestamp() / 1000));
  }
}

void keepTail(const char* line, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    s_tail.buf[s_tail.head] = line[i];
    s_tail.head = (s_tail.head + 1) % kTailBytes;
  }
  s_tail.len = s_tail.len + len > kTailBytes ? kTailBytes : s_tail.len + static_cast<uint32_t>(len);
}

/** stdout's writer: the serial port and the log. */
int teeWrite(void*, const char* data, int len) {
  if (len <= 0) return 0;
  std::fwrite(data, 1, static_cast<size_t>(len), s_console);
  std::fflush(s_console);
  char st[16];
  stamp(st, sizeof(st));
  core::syslog::append(data, static_cast<size_t>(len), st);
  return len;
}

/** ESP_LOG's printer. */
int logPrint(const char* fmt, va_list ap) {
  char buf[256];
  va_list copy;
  va_copy(copy, ap);
  int n = std::vsnprintf(buf, sizeof(buf), fmt, copy);
  va_end(copy);
  const int printed = std::vfprintf(s_console, fmt, ap);
  if (n > 0) {
    size_t len = static_cast<size_t>(n) < sizeof(buf) ? static_cast<size_t>(n) : sizeof(buf) - 1;
    if (static_cast<size_t>(n) >= sizeof(buf)) buf[len - 1] = '\n';  // cut short
    char st[16];
    stamp(st, sizeof(st));
    core::syslog::append(buf, len, st);
  }
  return printed;
}

}  // namespace

void logCaptureStart() {
  if (s_console != nullptr) return;
  // The previous run's last lines, if it ended badly.
  const esp_reset_reason_t why = esp_reset_reason();
  const bool unexpected = why == ESP_RST_PANIC || why == ESP_RST_INT_WDT || why == ESP_RST_TASK_WDT ||
                          why == ESP_RST_WDT || why == ESP_RST_CPU_LOCKUP;
  if (unexpected && s_tail.magic == kTailMagic && s_tail.head < kTailBytes && s_tail.len <= kTailBytes) {
    const size_t start = (s_tail.head + kTailBytes - s_tail.len) % kTailBytes;
    s_crash.reserve(s_tail.len);
    for (size_t i = 0; i < s_tail.len; ++i) {
      const char c = s_tail.buf[(start + i) % kTailBytes];
      s_crash.push_back(c == '\n' || (c >= 32 && c < 127) || (c & 0x80) ? c : '?');
    }
    const size_t nl = s_crash.find('\n');  // the first line is probably cut
    if (nl != std::string::npos && s_tail.len == kTailBytes) s_crash.erase(0, nl + 1);
  }
  s_tail.magic = kTailMagic;
  s_tail.head = 0;
  s_tail.len = 0;
  core::syslog::setLineSink(keepTail);

  s_console = _GLOBAL_REENT->_stdout;
  FILE* tee = fwopen(nullptr, teeWrite);
  if (tee != nullptr) {
    setvbuf(tee, nullptr, _IOLBF, 256);
    _GLOBAL_REENT->_stdout = tee;  // tasks started from now on
    stdout = tee;                  // this one
  }
  esp_log_set_vprintf(logPrint);
}

std::string takeCrashLog() {
  std::string out;
  out.swap(s_crash);
  return out;
}

}  // namespace plat
