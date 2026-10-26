#include <string>

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "platform/platform.h"

namespace plat {

namespace {

const char* TAG = "http";
constexpr size_t kMaxBody = 4 * 1024 * 1024;  // 250 nm over a busy region is ~1 MB

esp_err_t onHttpEvent(esp_http_client_event_t* ev) {
  if (ev->event_id == HTTP_EVENT_ON_DATA && ev->user_data != nullptr) {
    auto* body = static_cast<std::string*>(ev->user_data);
    if (body->size() + ev->data_len <= kMaxBody) {
      body->append(static_cast<const char*>(ev->data), ev->data_len);
    }
  }
  return ESP_OK;
}

}  // namespace

int httpGet(const char* url, std::string* body, int timeout_ms) {
  esp_http_client_config_t cfg = {};
  cfg.url = url;
  cfg.timeout_ms = timeout_ms;
  cfg.event_handler = onHttpEvent;
  cfg.user_data = body;
  cfg.crt_bundle_attach = esp_crt_bundle_attach;
  cfg.buffer_size = 8192;
  cfg.user_agent = "plane-radar-p4/1.0";

  esp_http_client_handle_t client = esp_http_client_init(&cfg);
  if (client == nullptr) return -1;
  body->reserve(64 * 1024);
  const esp_err_t err = esp_http_client_perform(client);
  int status = -1;
  if (err == ESP_OK) {
    status = esp_http_client_get_status_code(client);
  } else {
    ESP_LOGW(TAG, "GET failed: %s", esp_err_to_name(err));
  }
  esp_http_client_cleanup(client);
  return status;
}

}  // namespace plat
