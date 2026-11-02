// The system log. Everything printed on stdout (printf, and ESP_LOG from the
// tasks started after logCaptureStart()) goes to the serial port as before,
// and its bytes are copied into a ring buffer in RAM that survives a crash or
// watchdog restart. Nothing else happens while printing: no formatting, no
// clock, no allocation, no mutex. Printing happens on small-stack system
// tasks too (the event loop has about 2 KB), and v1.23.0 - v1.25.0, which did
// all that work right there, overflowed them at startup. logDrain() (UI task)
// later moves the new bytes into core::syslog with the time.

#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <sys/reent.h>
#include <unistd.h>

#include "core/syslog.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "platform/platform.h"

namespace plat {

namespace {

constexpr uint32_t kRingMagic = 0x4C4F4732;  // "LOG2"
constexpr uint32_t kRingBytes = 8192;        // a power of two

// Not cleared at startup: what the previous run printed last.
struct Ring {
  uint32_t magic;
  uint32_t head;  // bytes written ever (the buffer holds the last kRingBytes)
  char buf[kRingBytes];
};
__NOINIT_ATTR Ring s_ring;

portMUX_TYPE s_ring_mux = portMUX_INITIALIZER_UNLOCKED;
uint32_t s_read = 0;      // logDrain() has taken everything before this
int s_console_fd = -1;    // the serial port
std::string s_crash;
bool s_started = false;

void ringPut(const char* data, size_t len) {
  portENTER_CRITICAL(&s_ring_mux);
  for (size_t i = 0; i < len; ++i) s_ring.buf[(s_ring.head + i) & (kRingBytes - 1)] = data[i];
  s_ring.head += static_cast<uint32_t>(len);
  portEXIT_CRITICAL(&s_ring_mux);
}

/** stdout's writer: the serial port, and the ring. Keep it tiny. */
int teeWrite(void*, const char* data, int len) {
  if (len <= 0) return 0;
  if (s_console_fd >= 0) {
    int done = 0;
    while (done < len) {
      const int n = static_cast<int>(::write(s_console_fd, data + done, static_cast<size_t>(len - done)));
      if (n <= 0) break;
      done += n;
    }
  }
  ringPut(data, static_cast<size_t>(len));
  return len;
}

/** Drops terminal color codes and '\r', and anything unprintable (a half-overwritten byte after a crash). */
std::string printable(const std::string& in) {
  std::string out;
  bool code = false;
  for (char c : in) {
    if (code) {
      if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) code = false;
      continue;
    }
    if (c == '\x1b') {
      code = true;
      continue;
    }
    if (c == '\n' || (static_cast<unsigned char>(c) >= 32 && c != 127)) out.push_back(c);
  }
  return out;
}

}  // namespace

void logCaptureStart(bool capture) {
  if (s_started) return;
  s_started = true;
  // The previous run's last lines, if it ended badly.
  const esp_reset_reason_t why = esp_reset_reason();
  const bool unexpected = why == ESP_RST_PANIC || why == ESP_RST_INT_WDT || why == ESP_RST_TASK_WDT ||
                          why == ESP_RST_WDT || why == ESP_RST_CPU_LOCKUP;
  if (unexpected && s_ring.magic == kRingMagic) {
    const uint32_t n = s_ring.head < kRingBytes ? s_ring.head : kRingBytes;
    std::string raw;
    raw.reserve(n);
    for (uint32_t i = s_ring.head - n; i != s_ring.head; ++i) raw.push_back(s_ring.buf[i & (kRingBytes - 1)]);
    s_crash = printable(raw);
    const size_t nl = s_crash.find('\n');  // the first line is probably cut
    if (nl != std::string::npos && n == kRingBytes) s_crash.erase(0, nl + 1);
  }
  s_ring.magic = kRingMagic;
  s_ring.head = 0;
  s_read = 0;

  if (!capture) return;
  FILE* console = _GLOBAL_REENT->_stdout;
  std::fflush(console);
  s_console_fd = fileno(console);
  FILE* tee = fwopen(nullptr, teeWrite);
  if (tee == nullptr || s_console_fd < 0) return;  // no capture; printing works as before
  setvbuf(tee, nullptr, _IOLBF, 256);
  _GLOBAL_REENT->_stdout = tee;  // tasks started from now on (and ESP_LOG in them)
  stdout = tee;                  // this one
}

void logDrain() {
  char chunk[512];
  for (;;) {
    size_t n = 0;
    portENTER_CRITICAL(&s_ring_mux);
    const uint32_t head = s_ring.head;
    if (head - s_read > kRingBytes) s_read = head - kRingBytes;  // fell behind: the oldest is gone
    while (s_read != head && n < sizeof(chunk)) chunk[n++] = s_ring.buf[s_read++ & (kRingBytes - 1)];
    portEXIT_CRITICAL(&s_ring_mux);
    if (n == 0) return;
    char stamp[16];
    const time_t t = time(nullptr);
    if (t > 1600000000) {
      std::tm lt;
      localtime_r(&t, &lt);
      std::strftime(stamp, sizeof(stamp), "%H:%M:%S ", &lt);
    } else {
      std::snprintf(stamp, sizeof(stamp), "+%lus ", static_cast<unsigned long>(esp_log_timestamp() / 1000));
    }
    core::syslog::append(chunk, n, stamp);
  }
}

std::string takeCrashLog() {
  std::string out;
  out.swap(s_crash);
  return out;
}

}  // namespace plat
