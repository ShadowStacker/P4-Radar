// LVGL's memory: objects, styles, label texts and its small working buffers.
// With the C library's malloc, everything under 4 KB went to internal RAM
// (CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL), and a menu page is thousands of
// those. Internal RAM is what Wi-Fi's link to the ESP32-C6, TLS and the
// drivers need; when menus and downloads together used it up, Wi-Fi died
// ("eh_sdio: mempool OOM"). So LVGL gets PSRAM (cached, and plenty of it),
// with internal RAM only as a fallback. The display's draw buffer is
// allocated by the LVGL adapter itself and stays in internal RAM.
//
// Used through CONFIG_LV_USE_CUSTOM_MALLOC (sdkconfig.defaults).

#include "esp_heap_caps.h"
#include "lvgl.h"

#if LV_USE_STDLIB_MALLOC == LV_STDLIB_CUSTOM

namespace {
constexpr uint32_t kPsram = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
constexpr uint32_t kAny = MALLOC_CAP_8BIT;
}  // namespace

extern "C" {

void lv_mem_init(void) {}
void lv_mem_deinit(void) {}

lv_mem_pool_t lv_mem_add_pool(void* mem, size_t bytes) {
  (void)mem;
  (void)bytes;
  return nullptr;  // not supported (nor needed)
}

void lv_mem_remove_pool(lv_mem_pool_t pool) { (void)pool; }

void* lv_malloc_core(size_t size) {
  void* p = heap_caps_malloc(size, kPsram);
  return p != nullptr ? p : heap_caps_malloc(size, kAny);
}

void* lv_realloc_core(void* p, size_t new_size) {
  // heap_caps_realloc() leaves `p` as it was when it fails, so the fallback can try again.
  void* q = heap_caps_realloc(p, new_size, kPsram);
  return q != nullptr || new_size == 0 ? q : heap_caps_realloc(p, new_size, kAny);
}

void lv_free_core(void* p) { heap_caps_free(p); }

void lv_mem_monitor_core(lv_mem_monitor_t* mon) {
  if (mon == nullptr) return;
  multi_heap_info_t info;
  heap_caps_get_info(&info, kPsram);
  mon->total_size = info.total_free_bytes + info.total_allocated_bytes;
  mon->free_size = info.total_free_bytes;
  mon->free_biggest_size = info.largest_free_block;
  mon->free_cnt = info.free_blocks;
  mon->used_cnt = info.allocated_blocks;
  mon->max_used = info.total_free_bytes + info.total_allocated_bytes - info.minimum_free_bytes;
  mon->used_pct = mon->total_size ? static_cast<uint8_t>(100 - static_cast<uint64_t>(info.total_free_bytes) * 100 / mon->total_size) : 0;
  mon->frag_pct = 0;
}

lv_result_t lv_mem_test_core(void) { return LV_RESULT_OK; }

}  // extern "C"

#endif
