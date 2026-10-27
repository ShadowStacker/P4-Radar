// Settings persistence in NVS: one versioned blob for everything, plus the
// Wi-Fi credentials as separate keys so they survive a settings layout change.

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <vector>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "platform/platform.h"

namespace plat {

namespace {

const char* TAG = "storage";
constexpr char kNamespace[] = "radar";

// Size of Settings up to its last field as of v1.0.3, before fields were appended.
constexpr size_t kLegacySettingsSize = offsetof(core::Settings, poll_interval_s) + sizeof(uint8_t);

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

  // Older firmware saved a shorter Settings. Fields are only ever appended, so
  // copy the prefix that was saved and keep defaults for the rest.
  size_t len = 0;
  if (nvs_get_blob(h, "cfg", nullptr, &len) == ESP_OK && len >= offsetof(Blob, settings)) {
    std::vector<uint8_t> raw(len);
    if (nvs_get_blob(h, "cfg", raw.data(), &len) == ESP_OK) {
      uint16_t version, size;
      std::memcpy(&version, raw.data() + offsetof(Blob, version), sizeof(version));
      std::memcpy(&size, raw.data() + offsetof(Blob, size), sizeof(size));
      const size_t stored = std::min<size_t>(size, len - offsetof(Blob, settings));
      if (version == core::Settings::kVersion) {
        // Stop at the first appended field: an old blob's tail padding would
        // otherwise overwrite new fields with zeros instead of their defaults.
        const size_t copy = size == sizeof(core::Settings)
                                ? std::min(stored, sizeof(core::Settings))
                                : std::min(stored, kLegacySettingsSize);
        std::memcpy(static_cast<void*>(s), raw.data() + offsetof(Blob, settings), copy);
        if (copy < sizeof(core::Settings)) ESP_LOGI(TAG, "upgraded settings from older firmware");
      } else {
        ESP_LOGW(TAG, "settings version %u not supported; using defaults (Wi-Fi kept)", version);
      }
    }
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
  Blob blob{};
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
