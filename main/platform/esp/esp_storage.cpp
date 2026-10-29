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

/**
 * How much of a saved blob to trust, by its version. Fields past this point
 * keep their defaults. (Version 1 stops before autohide_buttons on purpose:
 * its default changed to on in v1.2.0.)
 */
size_t validPrefix(uint16_t version) {
  switch (version) {
    case 1:
      // v1.0.x and v1.1.0 blobs are the same size. In v1.0.x the byte that is
      // now military_only was zero-filled padding, which reads as its default.
      return offsetof(core::Settings, autohide_buttons);
    case 2: return offsetof(core::Settings, sweep_trail_deg);
    case 3: return offsetof(core::Settings, rotation);
    case 4: return offsetof(core::Settings, custom_hue);
    case 5: return offsetof(core::Settings, clock_mode);
    case 6: return offsetof(core::Settings, clock_smooth_seconds);
    case 7: return offsetof(core::Settings, map_style);
    case 8: return offsetof(core::Settings, wake_on_alarm);
    case 9: return offsetof(core::Settings, carto_key);
    case 10: return offsetof(core::Settings, wifi_saved);
    case 11: return offsetof(core::Settings, auto_update_check);
    case 12: return sizeof(core::Settings);
    default: return 0;
  }
}

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
      // Copy only fields that existed in the saved version; an old blob's tail
      // padding would otherwise overwrite newer fields with zeros.
      const size_t copy = std::min(stored, validPrefix(version));
      if (copy > 0) {
        std::memcpy(static_cast<void*>(s), raw.data() + offsetof(Blob, settings), copy);
        // v1.5.0 made miles the default; boards upgrading switch once.
        if (version < 5) s->units = core::Units::Mi;
        // v1.10.0 re-based rotation by 180 deg; keep the board's orientation.
        if (version < 10) s->rotation = static_cast<uint8_t>((s->rotation + 2) & 3);
        if (version != core::Settings::kVersion) {
          ESP_LOGI(TAG, "upgraded settings from version %u", version);
        }
      } else {
        ESP_LOGW(TAG, "settings version %u not supported; using defaults (Wi-Fi kept)", version);
      }
    }
  }

  s->carto_key[sizeof(s->carto_key) - 1] = '\0';  // stored strings are never trusted
  s->type_filter[sizeof(s->type_filter) - 1] = '\0';
  for (core::SavedWifi& w : s->wifi_saved) {
    w.ssid[sizeof(w.ssid) - 1] = '\0';
    w.pass[sizeof(w.pass) - 1] = '\0';
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

void favoritesLoad(std::vector<core::Favorite>* out) {
  out->clear();
  nvs_handle_t h;
  if (nvs_open(kNamespace, NVS_READONLY, &h) != ESP_OK) return;
  size_t len = 0;
  if (nvs_get_blob(h, "favs", nullptr, &len) == ESP_OK && len % sizeof(core::Favorite) == 0) {
    out->resize(std::min(len / sizeof(core::Favorite), core::kMaxFavorites));
    len = out->size() * sizeof(core::Favorite);
    if (nvs_get_blob(h, "favs", out->data(), &len) != ESP_OK) out->clear();
    for (core::Favorite& f : *out) {  // never trust stored strings to be terminated
      f.hex[sizeof(f.hex) - 1] = f.label[sizeof(f.label) - 1] = f.type[sizeof(f.type) - 1] = '\0';
    }
  }
  nvs_close(h);
}

void favoritesSave(const std::vector<core::Favorite>& favs) {
  nvs_handle_t h;
  if (nvs_open(kNamespace, NVS_READWRITE, &h) != ESP_OK) return;
  if (favs.empty()) {
    nvs_erase_key(h, "favs");
  } else {
    nvs_set_blob(h, "favs", favs.data(), favs.size() * sizeof(core::Favorite));
  }
  if (nvs_commit(h) != ESP_OK) ESP_LOGE(TAG, "nvs_commit failed (favorites)");
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
