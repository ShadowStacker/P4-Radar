#include <cstring>
#include <string>

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "platform/platform.h"

namespace plat {

namespace {

const char* TAG = "http";
constexpr size_t kMaxBody = 4 * 1024 * 1024;  // 250 nm over a busy region is ~1 MB
// Tile servers ask apps to identify themselves.
const char* kUserAgent = "plane-radar-p4/1.8 (+https://github.com/ShadowStacker/P4-Radar)";

// Persistent client for httpGetKeepAlive(), and the host it is connected to.
esp_http_client_handle_t s_ka_client = nullptr;
std::string s_ka_host;

std::string hostOf(const char* url) {
  const char* p = std::strstr(url, "://");
  p = p ? p + 3 : url;
  const char* e = std::strchr(p, '/');
  return e ? std::string(p, e - p) : std::string(p);
}

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

int httpGet(const char* url, std::string* body, int timeout_ms, const char* const* headers) {
  esp_http_client_config_t cfg = {};
  cfg.url = url;
  cfg.timeout_ms = timeout_ms;
  cfg.event_handler = onHttpEvent;
  cfg.user_data = body;
  cfg.crt_bundle_attach = esp_crt_bundle_attach;
  cfg.buffer_size = 8192;
  cfg.user_agent = kUserAgent;

  esp_http_client_handle_t client = esp_http_client_init(&cfg);
  if (client == nullptr) return -1;
  for (const char* const* h = headers; h != nullptr && h[0] != nullptr && h[1] != nullptr; h += 2) {
    esp_http_client_set_header(client, h[0], h[1]);
  }
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

namespace plat {

int httpGetKeepAlive(const char* url, std::string* body, int timeout_ms) {
  const std::string host = hostOf(url);
  if (s_ka_client != nullptr && host != s_ka_host) {
    esp_http_client_cleanup(s_ka_client);
    s_ka_client = nullptr;
  }
  if (s_ka_client == nullptr) {
    esp_http_client_config_t cfg = {};
    cfg.url = url;
    cfg.timeout_ms = timeout_ms;
    cfg.event_handler = onHttpEvent;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.buffer_size = 8192;
    cfg.user_agent = kUserAgent;
    cfg.keep_alive_enable = true;
    s_ka_client = esp_http_client_init(&cfg);
    if (s_ka_client == nullptr) return -1;
    s_ka_host = host;
  } else {
    esp_http_client_set_url(s_ka_client, url);
  }
  esp_http_client_set_user_data(s_ka_client, body);
  const esp_err_t err = esp_http_client_perform(s_ka_client);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "GET %s failed: %s", url, esp_err_to_name(err));
    esp_http_client_cleanup(s_ka_client);  // start fresh next time
    s_ka_client = nullptr;
    return -1;
  }
  return esp_http_client_get_status_code(s_ka_client);
}

}  // namespace plat
