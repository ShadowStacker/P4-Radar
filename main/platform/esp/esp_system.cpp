#include <atomic>
#include <algorithm>
#include <cstdio>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include <sys/time.h>
#include <ctime>

#include "esp_app_desc.h"
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "platform/platform.h"

namespace plat {

namespace {

void taskTrampoline(void* arg) {
  auto* fn = static_cast<std::function<void()>*>(arg);
  (*fn)();
  delete fn;
  vTaskDelete(nullptr);
}

}  // namespace

int64_t unixTimeMs() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return static_cast<int64_t>(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}

// The board has no battery-backed clock, so before SNTP it counts from 1970.
bool timeValid() { return std::time(nullptr) > 1735689600; }  // 2025-01-01

uint64_t micros() { return static_cast<uint64_t>(esp_timer_get_time()); }

uint32_t millis() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }

void startTask(const char* name, std::function<void()> fn, size_t stack_bytes, int priority) {
  auto* heap_fn = new std::function<void()>(std::move(fn));
  // Network work stays on core 0; LVGL renders on core 1.
  xTaskCreatePinnedToCore(taskTrampoline, name, stack_bytes, heap_fn, priority, nullptr, 0);
}

void sleepMs(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

void restart() { esp_restart(); }

namespace {
// Kept across a software restart (not across power off): restarts in a row
// for a dead internet connection.
RTC_NOINIT_ATTR uint32_t s_net_restart_magic;
RTC_NOINIT_ATTR uint32_t s_net_restart_count;
constexpr uint32_t kNetRestartMagic = 0x4E455452;  // "NETR"
}  // namespace

int netRestarts() { return s_net_restart_magic == kNetRestartMagic ? static_cast<int>(s_net_restart_count) : 0; }

void netRestartsClear() {
  if (netRestarts() == 0) return;
  s_net_restart_count = 0;
}

void restartForNetwork() {
  const int n = netRestarts();
  s_net_restart_magic = kNetRestartMagic;
  s_net_restart_count = static_cast<uint32_t>(n + 1);
  esp_restart();
}

const char* firmwareVersion() { return esp_app_get_description()->version; }

std::atomic<uint32_t> s_large_refused{0};

void* allocLarge(size_t bytes) {
  // PSRAM only. Falling back to internal RAM when PSRAM is short used to let
  // image caches eat the RAM Wi-Fi needs (and the link then died); callers
  // cope with nullptr (an icon or tile is skipped instead).
  void* p = heap_caps_aligned_alloc(64, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (p == nullptr) s_large_refused.fetch_add(1, std::memory_order_relaxed);
  return p;
}

uint32_t largeAllocsRefused() { return s_large_refused.load(std::memory_order_relaxed); }

void freeLarge(void* p) { heap_caps_free(p); }

size_t freeInternalHeap() { return heap_caps_get_free_size(MALLOC_CAP_INTERNAL); }

size_t freePsram() { return heap_caps_get_free_size(MALLOC_CAP_SPIRAM); }

Health sampleHealth(int slot) {
  // Only counters that are cheap to read. No heap walk (largest free block)
  // and no uxTaskGetSystemState(): that one runs with interrupts off and
  // checks every task's stack byte by byte (the LVGL task's is in PSRAM),
  // long enough to upset the SDIO link to the Wi-Fi chip. With the
  // on-screen stats sampling every second, Wi-Fi died within minutes.
  Health h;
  h.ram_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  h.ram_min = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
  h.psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
#if configUSE_TRACE_FACILITY && configGENERATE_RUN_TIME_STATS
  // CPU time since this slot's previous sample. The counters are 32-bit
  // microseconds by default and wrap every ~71 min; unsigned differences in
  // the counter's own type stay right across a wrap.
  using Counter = configRUN_TIME_COUNTER_TYPE;
  // Tasks worth naming. Looked up by name each time (a stopped web server's
  // task is gone); vTaskGetInfo() without the stack check is quick.
  static const char* const kTasks[] = {"lvgl", "adsb", "card", "wifi", "httpd", "tiT", "sys_evt", "esp_timer",
                                       "sdio_read", "sdio_write", "sdio_process_rx", "sdio_rx_buf"};
  constexpr size_t kN = sizeof(kTasks) / sizeof(kTasks[0]);
  struct Baseline {
    Counter tasks[kN] = {};
    Counter idle[2] = {};
    Counter total = 0;
    bool valid = false;
  };
  static Baseline s_base[2];
  Baseline& base = s_base[slot & 1];
  const Counter total = static_cast<Counter>(portGET_RUN_TIME_COUNTER_VALUE());
  Counter idle[2];
  for (int c = 0; c < 2; ++c) idle[c] = ulTaskGetIdleRunTimeCounterForCore(c);
  Counter now[kN];
  bool found[kN];
  for (size_t i = 0; i < kN; ++i) {
    now[i] = 0;
    found[i] = false;
    if (TaskHandle_t t = xTaskGetHandle(kTasks[i])) {
      TaskStatus_t st;
      vTaskGetInfo(t, &st, pdFALSE, eRunning);  // no stack check, no state lookup
      now[i] = st.ulRunTimeCounter;
      found[i] = true;
    }
  }
  const uint64_t elapsed = static_cast<Counter>(total - base.total);
  if (base.valid && elapsed > 0) {
    auto pct = [&](uint64_t t) { return static_cast<int>(std::min<uint64_t>(100, t * 100 / elapsed)); };
    for (int c = 0; c < 2; ++c) h.cpu[c] = 100 - pct(static_cast<Counter>(idle[c] - base.idle[c]));
    struct Use {
      const char* name;
      uint64_t time;
    };
    std::vector<Use> uses;
    for (size_t i = 0; i < kN; ++i) {
      if (found[i]) uses.push_back({kTasks[i], static_cast<Counter>(now[i] - base.tasks[i])});
    }
    std::sort(uses.begin(), uses.end(), [](const Use& a, const Use& b) { return a.time > b.time; });
    char buf[48];
    for (size_t i = 0; i < uses.size() && i < 5; ++i) {
      std::snprintf(buf, sizeof(buf), "%s%s %d%%", i ? ", " : "", uses[i].name, pct(uses[i].time));
      h.top += buf;
    }
  }
  for (size_t i = 0; i < kN; ++i) base.tasks[i] = now[i];
  base.idle[0] = idle[0];
  base.idle[1] = idle[1];
  base.total = total;
  base.valid = true;
#endif
  return h;
}

}  // namespace plat
