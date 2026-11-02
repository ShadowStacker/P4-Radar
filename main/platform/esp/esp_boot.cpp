// Start-up bookkeeping: why the board restarted, how often, and the
// emergency rollback (three quick restarts go back to the previous version).

#include <cstdio>
#include <cstring>

#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "nvs.h"
#include "platform/esp/esp_platform.h"
#include "platform/platform.h"

namespace plat {

namespace {

const char* TAG = "boot";
constexpr char kNamespace[] = "boot";
constexpr uint8_t kQuickBootsForRollback = 3;

BootInfo s_info;

const char* reasonText(esp_reset_reason_t r, bool* unexpected) {
  *unexpected = false;
  switch (r) {
    case ESP_RST_POWERON: return "Power on";
    case ESP_RST_SW: return "Restarted by the radar";
    case ESP_RST_EXT: return "Reset button";
    case ESP_RST_DEEPSLEEP: return "Woke from sleep";
    case ESP_RST_USB: return "Reset over USB";
    case ESP_RST_JTAG: return "Reset by debugger";
    case ESP_RST_PANIC: *unexpected = true; return "Crash";
    case ESP_RST_INT_WDT: *unexpected = true; return "Watchdog (interrupts)";
    case ESP_RST_TASK_WDT: *unexpected = true; return "Watchdog (task)";
    case ESP_RST_WDT: *unexpected = true; return "Watchdog";
    case ESP_RST_BROWNOUT: *unexpected = true; return "Power dip (brownout)";
    case ESP_RST_CPU_LOCKUP: *unexpected = true; return "CPU lockup";
    default: *unexpected = true; return "Unknown";
  }
}

uint32_t getU32(nvs_handle_t h, const char* key) {
  uint32_t v = 0;
  nvs_get_u32(h, key, &v);
  return v;
}

}  // namespace

namespace esp {

void bootCheck() {
  s_info.reset_reason = reasonText(esp_reset_reason(), &s_info.unexpected);
  nvs_handle_t h;
  if (nvs_open(kNamespace, NVS_READWRITE, &h) != ESP_OK) return;
  s_info.boots = getU32(h, "boots") + 1;
  s_info.unexpected_resets = getU32(h, "unexpected") + (s_info.unexpected ? 1 : 0);
  uint8_t quick = 0;
  nvs_get_u8(h, "quick", &quick);
  ++quick;
  size_t n = sizeof(s_info.rolled_back_from);
  if (nvs_get_str(h, "rolledback", s_info.rolled_back_from, &n) != ESP_OK) s_info.rolled_back_from[0] = '\0';
  nvs_erase_key(h, "rolledback");  // reported once, on the start after the rollback

  const char* running_version = esp_app_get_description()->version;
  ESP_LOGI(TAG, "start %u (%s), %u unexpected so far, quick start %u of %u", static_cast<unsigned>(s_info.boots),
           s_info.reset_reason, static_cast<unsigned>(s_info.unexpected_resets), quick, kQuickBootsForRollback);

  if (quick >= kQuickBootsForRollback) {
    quick = 0;
    const esp_partition_t* other = esp_ota_get_next_update_partition(nullptr);
    esp_app_desc_t desc;
    if (other != nullptr && esp_ota_get_partition_description(other, &desc) == ESP_OK &&
        std::strcmp(desc.project_name, esp_app_get_description()->project_name) == 0 &&
        esp_ota_set_boot_partition(other) == ESP_OK) {
      ESP_LOGW(TAG, "three quick restarts: going back from %s to %s", running_version, desc.version);
      nvs_set_str(h, "rolledback", running_version);
      nvs_set_u8(h, "quick", 0);
      nvs_set_u32(h, "boots", s_info.boots);
      nvs_set_u32(h, "unexpected", s_info.unexpected_resets);
      nvs_commit(h);
      nvs_close(h);
      esp_restart();
    }
    ESP_LOGW(TAG, "three quick restarts, but there's no other version to go back to");
  }
  nvs_set_u8(h, "quick", quick);
  nvs_set_u32(h, "boots", s_info.boots);
  nvs_set_u32(h, "unexpected", s_info.unexpected_resets);
  nvs_commit(h);
  nvs_close(h);
}

}  // namespace esp

BootInfo bootInfo() { return s_info; }

void bootStable() {
  nvs_handle_t h;
  if (nvs_open(kNamespace, NVS_READWRITE, &h) != ESP_OK) return;
  nvs_set_u8(h, "quick", 0);
  nvs_commit(h);
  nvs_close(h);
}

}  // namespace plat
