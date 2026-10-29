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

// Persistent clients for httpGetKeepAlive(), one per host (the poller
// alternates between the ADS-B feed and map tile servers). Each kept
// connection saves a TLS handshake: a few hundred ms of CPU on the network core.
// (TLS buffers are in PSRAM: CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC.)
struct KeptClient {
  std::string host;
  esp_http_client_handle_t client = nullptr;
  uint32_t last_used = 0;
};
constexpr int kMaxKeptClients = 4;  // feed, map tiles, weather list, weather tiles
KeptClient s_kept[kMaxKeptClients];
uint32_t s_use_counter = 0;

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

namespace {

/** The kept client for `host`, creating it (in a free or the oldest slot) if needed. */
KeptClient* keptClientFor(const std::string& host, const char* url, int timeout_ms, bool* fresh) {
  for (KeptClient& k : s_kept) {
    if (k.client != nullptr && k.host == host) {
      esp_http_client_set_url(k.client, url);
      *fresh = false;
      return &k;
    }
  }
  KeptClient* slot = &s_kept[0];
  for (KeptClient& k : s_kept) {
    if (k.client == nullptr) {
      slot = &k;
      break;
    }
    if (k.last_used < slot->last_used) slot = &k;
  }
  if (slot->client != nullptr) esp_http_client_cleanup(slot->client);
  esp_http_client_config_t cfg = {};
  cfg.url = url;
  cfg.timeout_ms = timeout_ms;
  cfg.event_handler = onHttpEvent;
  cfg.crt_bundle_attach = esp_crt_bundle_attach;
  cfg.buffer_size = 8192;
  cfg.user_agent = kUserAgent;
  cfg.keep_alive_enable = true;
  slot->client = esp_http_client_init(&cfg);
  slot->host = host;
  *fresh = true;
  return slot->client != nullptr ? slot : nullptr;
}

}  // namespace

int httpGetKeepAlive(const char* url, std::string* body, int timeout_ms) {
  const std::string host = hostOf(url);
  for (int attempt = 0; attempt < 2; ++attempt) {
    bool fresh = false;
    KeptClient* slot = keptClientFor(host, url, timeout_ms, &fresh);
    if (slot == nullptr) return -1;
    slot->last_used = ++s_use_counter;
    body->clear();
    esp_http_client_set_user_data(slot->client, body);
    const esp_err_t err = esp_http_client_perform(slot->client);
    if (err == ESP_OK) return esp_http_client_get_status_code(slot->client);
    esp_http_client_cleanup(slot->client);  // start fresh next time
    slot->client = nullptr;
    // A kept connection the server has since closed fails once; that's not a
    // real failure, so try again on a new connection.
    if (fresh) {
      ESP_LOGW(TAG, "GET %s failed: %s", url, esp_err_to_name(err));
      return -1;
    }
  }
  return -1;
}

}  // namespace plat
