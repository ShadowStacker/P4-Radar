#include <algorithm>
#include <cstdio>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include <sys/time.h>
#include <ctime>

#include "esp_app_desc.h"
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

const char* firmwareVersion() { return esp_app_get_description()->version; }

void* allocLarge(size_t bytes) {
  void* p = heap_caps_aligned_alloc(64, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return p != nullptr ? p : heap_caps_aligned_alloc(64, bytes, MALLOC_CAP_8BIT);
}

void freeLarge(void* p) { heap_caps_free(p); }

size_t freeInternalHeap() { return heap_caps_get_free_size(MALLOC_CAP_INTERNAL); }

size_t freePsram() { return heap_caps_get_free_size(MALLOC_CAP_SPIRAM); }

Health sampleHealth(int slot) {
  Health h;
  h.ram_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  h.ram_min = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
  h.ram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
  h.psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
#if configUSE_TRACE_FACILITY && configGENERATE_RUN_TIME_STATS
  // CPU time per task since this slot's previous sample. The counters are
  // 32-bit microseconds by default and wrap every ~71 min; unsigned
  // differences in the counter's own type stay right across a wrap.
  using Counter = configRUN_TIME_COUNTER_TYPE;
  struct Baseline {
    std::unordered_map<TaskHandle_t, Counter> tasks;
    Counter total = 0;
    bool valid = false;
  };
  static Baseline s_base[2];
  Baseline& base = s_base[slot & 1];
  const UBaseType_t n = uxTaskGetNumberOfTasks();
  std::vector<TaskStatus_t> tasks(n + 4);
  Counter total = 0;
  tasks.resize(uxTaskGetSystemState(tasks.data(), tasks.size(), &total));
  const uint64_t elapsed = static_cast<Counter>(total - base.total);
  if (base.valid && elapsed > 0) {
    struct Use {
      const char* name;
      uint64_t time;
    };
    std::vector<Use> uses;
    uint64_t idle[2] = {0, 0};
    for (const TaskStatus_t& t : tasks) {
      auto it = base.tasks.find(t.xHandle);
      const uint64_t d = it == base.tasks.end() ? 0 : static_cast<Counter>(t.ulRunTimeCounter - it->second);
      bool is_idle = false;
      for (int c = 0; c < 2; ++c) {
        if (t.xHandle == xTaskGetIdleTaskHandleForCore(c)) {
          idle[c] = d;
          is_idle = true;
        }
      }
      if (!is_idle) uses.push_back({t.pcTaskName, d});
    }
    std::sort(uses.begin(), uses.end(), [](const Use& a, const Use& b) { return a.time > b.time; });
    auto pct = [&](uint64_t t) { return static_cast<int>(std::min<uint64_t>(100, t * 100 / elapsed)); };
    h.cpu[0] = 100 - pct(idle[0]);
    h.cpu[1] = 100 - pct(idle[1]);
    char buf[48];
    for (size_t i = 0; i < uses.size() && i < 5; ++i) {
      std::snprintf(buf, sizeof(buf), "%s%s %d%%", i ? ", " : "", uses[i].name, pct(uses[i].time));
      h.top += buf;
    }
  }
  base.tasks.clear();
  for (const TaskStatus_t& t : tasks) base.tasks[t.xHandle] = t.ulRunTimeCounter;
  base.total = total;
  base.valid = true;
#endif
  return h;
}

}  // namespace plat
