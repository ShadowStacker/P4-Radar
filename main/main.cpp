/**
 * Plane Radar P4 — live ADS-B radar on the Waveshare ESP32-P4 round touch
 * display. Boot order: storage -> display/touch -> audio -> Wi-Fi -> settings
 * -> UI -> ADS-B poller.
 */

#include <cstdlib>

#include "app/app.h"
#include "core/rotation.h"
#include "cJSON.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "platform/esp/esp_platform.h"
#include "platform/platform.h"
#include "ui/display_rotation.h"
#include "ui/ui.h"

namespace {

// A wide-area ADS-B response becomes tens of thousands of small cJSON nodes;
// keep them in PSRAM so parsing never starves Wi-Fi or LVGL of internal RAM.
void* psramMalloc(size_t size) {
  void* p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return p != nullptr ? p : malloc(size);
}

}  // namespace

extern "C" void app_main(void) {
  cJSON_Hooks hooks = {psramMalloc, free};
  cJSON_InitHooks(&hooks);

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  // The orientation is fixed when the display starts, so peek at it first.
  core::Settings boot_settings;
  plat::settingsLoad(&boot_settings);
  const int quarter_turns = core::panelQuarterTurns(boot_settings.rotation);
  lv_display_t* disp = plat::esp::displayStart(quarter_turns);
  if (plat::esp::lvglLock()) {
    ui::installDisplayRotation(disp, quarter_turns);
    plat::esp::lvglUnlock();
  }
  // Audio first, while the screen is still dark: it was the moment of the one
  // cyan flash left at startup (the codec, speaker amp and sound synthesis
  // start here), and nothing needs to be shown for it.
  plat::esp::audioStart();
  plat::esp::bootScreen("Starting Wi-Fi module...");
  if (!plat::esp::wifiStart()) {
    plat::esp::bootScreen("Wi-Fi module not responding");
    vTaskDelay(pdMS_TO_TICKS(1500));  // let the message be read; the radar still runs
  }
  plat::esp::bootScreen("Loading...");
  app::init();  // loads settings, sets brightness/volume, starts connecting

  if (plat::esp::lvglLock()) {
    lv_obj_t* boot = lv_screen_active();
    ui::init(disp);
    if (boot != lv_screen_active()) lv_obj_delete(boot);
    plat::esp::lvglUnlock();
  }
  ESP_LOGI("main", "heap after UI: internal %u KB free (min %u KB, largest %u KB), PSRAM %u KB free",
           static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
           static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL) / 1024),
           static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024),
           static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
  app::sound(core::Sound::Startup);
  app::startNetworkTask();
  ESP_LOGI("main", "Plane Radar %s running", plat::firmwareVersion());
}
