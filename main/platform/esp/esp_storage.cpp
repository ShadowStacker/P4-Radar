// Settings persistence in NVS: one versioned blob for everything, plus the
// Wi-Fi credentials as separate keys so they survive a settings layout change.

#include <cstring>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "platform/platform.h"

namespace plat {

namespace {

const char* TAG = "storage";
constexpr char kNamespace[] = "radar";

struct Blob {
  uint16_t version;
  uint16_t size;
  core::Settings settings;
};

}  // namespace

void settingsLoad(core::Settings* s) {
  *s = core::Settings{};
  nvs_handle_t h;
  if (nvs_open(kNamespace, NVS_READONLY, &h) != ESP_OK) return;  // first boot

  Blob blob;
  size_t len = sizeof(blob);
  if (nvs_get_blob(h, "cfg", &blob, &len) == ESP_OK && len == sizeof(blob) &&
      blob.version == core::Settings::kVersion && blob.size == sizeof(core::Settings)) {
    *s = blob.settings;
  } else if (len != sizeof(blob)) {
    ESP_LOGW(TAG, "settings layout changed; using defaults (Wi-Fi kept)");
  }

  size_t n = sizeof(s->wifi_ssid);
  if (nvs_get_str(h, "ssid", s->wifi_ssid, &n) != ESP_OK) s->wifi_ssid[0] = '\0';
  n = sizeof(s->wifi_pass);
  if (nvs_get_str(h, "pass", s->wifi_pass, &n) != ESP_OK) s->wifi_pass[0] = '\0';
  nvs_close(h);
}

void settingsSave(const core::Settings& s) {
  nvs_handle_t h;
  if (nvs_open(kNamespace, NVS_READWRITE, &h) != ESP_OK) {
    ESP_LOGE(TAG, "nvs_open failed");
    return;
  }
  Blob blob;
  std::memset(&blob, 0, sizeof(blob));
  blob.version = core::Settings::kVersion;
  blob.size = sizeof(core::Settings);
  blob.settings = s;
  nvs_set_blob(h, "cfg", &blob, sizeof(blob));
  nvs_set_str(h, "ssid", s.wifi_ssid);
  nvs_set_str(h, "pass", s.wifi_pass);
  if (nvs_commit(h) != ESP_OK) ESP_LOGE(TAG, "nvs_commit failed");
  nvs_close(h);
}

void settingsErase() {
  nvs_handle_t h;
  if (nvs_open(kNamespace, NVS_READWRITE, &h) != ESP_OK) return;
  nvs_erase_all(h);
  nvs_commit(h);
  nvs_close(h);
}

}  // namespace plat
