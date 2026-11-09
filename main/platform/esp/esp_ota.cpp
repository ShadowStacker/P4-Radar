// Firmware updates over Wi-Fi into the idle OTA slot (see partitions.csv).

#include <cstring>
#include <string>

#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "core/fwcheck.h"
#include "platform/platform.h"
#include "sdkconfig.h"

namespace plat {

namespace {
const char* TAG = "ota";

// The HTTP status of the last response (the download link, then GitHub's
// redirect target), so a failed start can say what the server answered.
int s_last_status = 0;

esp_err_t onHttpEvent(esp_http_client_event_t* evt) {
  if (evt->event_id == HTTP_EVENT_ON_HEADER) s_last_status = esp_http_client_get_status_code(evt->client);
  return ESP_OK;
}

/** Why the download couldn't start, in words (fits UpdateStatus::error). */
std::string beginError(esp_err_t err, int status) {
  if (status >= 500) {
    return "GitHub's server is busy (HTTP " + std::to_string(status) + ") - try again later";
  }
  if (status == 404) return "Update file not found on GitHub (HTTP 404)";
  if (status >= 400) return "GitHub refused the download (HTTP " + std::to_string(status) + ")";
  if (status == 0) return std::string("Couldn't reach GitHub (") + esp_err_to_name(err) + ")";
  return std::string("Download failed (") + esp_err_to_name(err) + ")";
}
}

namespace {
#if CONFIG_BSP_LCD_TYPE_720_720_4_INCH
#define RADAR_BOARD "4in-720x720"
#else
#define RADAR_BOARD "3.4in-800x800"
#endif
#if CONFIG_ESP32P4_SELECTS_REV_LESS_V3 || (defined(CONFIG_ESP32P4_REV_MIN_FULL) && CONFIG_ESP32P4_REV_MIN_FULL < 300)
#define RADAR_CHIP "-chip-v1"
#else
#define RADAR_CHIP "-chip-v3"
#endif
// Marks the board in the image, so a file uploaded from the phone page for
// another board is refused (core::FirmwareCheck looks for it).
const char kBoard[] = "PLANE-RADAR-BOARD=" RADAR_BOARD RADAR_CHIP;
static_assert(sizeof("PLANE-RADAR-BOARD=") == sizeof(core::kBoardMarker), "same marker as core/fwcheck.h");
#undef RADAR_BOARD
#undef RADAR_CHIP
}  // namespace

const char* firmwareVariant() { return kBoard + sizeof(core::kBoardMarker) - 1; }

bool installFirmware(const char* url, const std::function<void(size_t, size_t)>& progress, std::string* error) {
  if (esp_ota_get_next_update_partition(nullptr) == nullptr) {
    *error = "No update slot - flash this version over USB once";
    return false;
  }
  esp_http_client_config_t http = {};
  http.url = url;
  http.crt_bundle_attach = esp_crt_bundle_attach;
  http.timeout_ms = 20000;
  http.keep_alive_enable = true;
  // GitHub answers with a redirect to a long signed URL; the default 512-byte
  // request buffer can't hold it.
  http.buffer_size = 8192;
  http.buffer_size_tx = 4096;
  http.user_agent = "PlaneRadarP4";
  http.event_handler = onHttpEvent;
  esp_https_ota_config_t cfg = {};
  cfg.http_config = &http;

  // GitHub's download servers sometimes time out (504) or drop the
  // connection for a while; a few tries a little apart usually get through.
  // A "no" (4xx) won't change on a retry.
  static constexpr int kTries = 4;
  static constexpr int kWaitMs[kTries - 1] = {3000, 8000, 15000};
  esp_https_ota_handle_t ota = nullptr;
  esp_err_t err = ESP_FAIL;
  for (int attempt = 0; attempt < kTries; ++attempt) {
    s_last_status = 0;
    err = esp_https_ota_begin(&cfg, &ota);
    if (err == ESP_OK) break;
    ESP_LOGW(TAG, "begin %s: %s (HTTP %d), try %d of %d", url, esp_err_to_name(err), s_last_status, attempt + 1, kTries);
    const bool refused = s_last_status >= 400 && s_last_status < 500;
    if (refused || attempt == kTries - 1) {
      *error = beginError(err, s_last_status);
      return false;
    }
    progress(0, 0);
    vTaskDelay(pdMS_TO_TICKS(kWaitMs[attempt]));
  }
  esp_app_desc_t desc;
  if (esp_https_ota_get_img_desc(ota, &desc) != ESP_OK ||
      std::strcmp(desc.project_name, esp_app_get_description()->project_name) != 0) {
    esp_https_ota_abort(ota);
    *error = "That file isn't Plane Radar firmware";
    return false;
  }
  ESP_LOGI(TAG, "installing %s %s from %s", desc.project_name, desc.version, url);
  const int total = esp_https_ota_get_image_size(ota);
  while ((err = esp_https_ota_perform(ota)) == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
    const int done = esp_https_ota_get_image_len_read(ota);
    progress(done > 0 ? done : 0, total > 0 ? total : 0);
  }
  if (err != ESP_OK || !esp_https_ota_is_complete_data_received(ota)) {
    ESP_LOGW(TAG, "download: %s", esp_err_to_name(err));
    esp_https_ota_abort(ota);
    *error = "Download interrupted - try again";
    return false;
  }
  // Checks the image (chip, revision, checksum) and selects it for the next boot.
  err = esp_https_ota_finish(ota);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "finish: %s", esp_err_to_name(err));
    *error = err == ESP_ERR_OTA_VALIDATE_FAILED ? "The download is damaged or for another board"
                                                : std::string("Install failed (") + esp_err_to_name(err) + ")";
    return false;
  }
  progress(total > 0 ? total : 1, total > 0 ? total : 1);
  ESP_LOGI(TAG, "installed; boots on restart");
  return true;
}

void confirmFirmware() {
  esp_ota_img_states_t state;
  const esp_partition_t* running = esp_ota_get_running_partition();
  if (running != nullptr && esp_ota_get_state_partition(running, &state) == ESP_OK &&
      state == ESP_OTA_IMG_PENDING_VERIFY) {
    esp_ota_mark_app_valid_cancel_rollback();
    ESP_LOGI(TAG, "new firmware confirmed");
  }
}

}  // namespace plat
