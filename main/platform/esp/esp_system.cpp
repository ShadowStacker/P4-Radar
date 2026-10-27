#include <functional>

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

}  // namespace plat
