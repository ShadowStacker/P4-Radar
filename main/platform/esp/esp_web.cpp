// The phone page's web server (esp_http_server on port 80), its name on the
// network (mDNS: plane-radar.local), and writing an uploaded firmware file.

#include <cstdio>
#include <cstring>
#include <string>

#include "esp_event.h"
#include "esp_http_server.h"
#include "lwip/sockets.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_random.h"
#include "mdns.h"
#include "platform/platform.h"

namespace plat {

namespace {

const char* TAG = "web";

httpd_handle_t s_server = nullptr;
WebHandler s_handler;
bool s_mdns = false;
WebStats s_stats;  // written by the server task, read by the UI (a torn read only garbles a label)

esp_ota_handle_t s_ota = 0;
const esp_partition_t* s_ota_part = nullptr;

std::string header(httpd_req_t* req, const char* name) {
  const size_t n = httpd_req_get_hdr_value_len(req, name);
  if (n == 0 || n > 4096) return "";
  std::string v(n + 1, '\0');
  if (httpd_req_get_hdr_value_str(req, name, &v[0], v.size()) != ESP_OK) return "";
  v.resize(n);
  return v;
}

const char* statusLine(int code) {
  switch (code) {
    case 200: return "200 OK";
    case 400: return "400 Bad Request";
    case 401: return "401 Unauthorized";
    case 403: return "403 Forbidden";
    case 404: return "404 Not Found";
    case 409: return "409 Conflict";
    case 429: return "429 Too Many Requests";
    case 503: return "503 Service Unavailable";
    default: return "500 Internal Server Error";
  }
}

/** The other end's IPv4 address ("" if it's the radar itself). */
std::string peer(httpd_req_t* req) {
  sockaddr_in6 a = {};
  socklen_t len = sizeof(a);
  if (getpeername(httpd_req_to_sockfd(req), reinterpret_cast<sockaddr*>(&a), &len) != 0) return "?";
  char ip[48] = "";
  if (a.sin6_family == AF_INET) {
    inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(&a)->sin_addr, ip, sizeof(ip));
  } else {
    // IPv4 mapped into IPv6 (::ffff:a.b.c.d): the last four bytes.
    inet_ntop(AF_INET, reinterpret_cast<const uint8_t*>(&a.sin6_addr) + 12, ip, sizeof(ip));
  }
  return std::strcmp(ip, "127.0.0.1") == 0 ? "" : ip;
}

esp_err_t onRequest(httpd_req_t* req) {
  const std::string from = peer(req);
  ESP_LOGI(TAG, "%s %s from %s", req->method == HTTP_POST ? "POST" : "GET", req->uri,
           from.empty() ? "the radar" : from.c_str());
  if (!from.empty()) {
    ++s_stats.requests;
    std::snprintf(s_stats.last_client, sizeof(s_stats.last_client), "%s", from.c_str());
    s_stats.last_ms = millis();
  }
  WebRequest r;
  r.method = req->method == HTTP_POST ? "POST" : "GET";
  const char* q = std::strchr(req->uri, '?');
  r.path = q ? std::string(req->uri, q - req->uri) : std::string(req->uri);
  if (q) r.query = q + 1;
  r.cookie = header(req, "Cookie");
  r.content_length = req->content_len;
  if (req->content_len <= kWebMaxBody) {
    r.body.resize(req->content_len);
    size_t got = 0;
    while (got < req->content_len) {
      const int n = httpd_req_recv(req, &r.body[got], req->content_len - got);
      if (n == HTTPD_SOCK_ERR_TIMEOUT) continue;
      if (n <= 0) return ESP_FAIL;
      got += static_cast<size_t>(n);
    }
  } else {
    r.read = [req](char* buf, size_t len) -> int {
      for (int tries = 0; tries < 5; ++tries) {
        const int n = httpd_req_recv(req, buf, len);
        if (n != HTTPD_SOCK_ERR_TIMEOUT) return n < 0 ? -1 : n;
      }
      return -1;
    };
  }
  const WebResponse res = s_handler(r);
  httpd_resp_set_status(req, statusLine(res.status));
  httpd_resp_set_type(req, res.type.c_str());
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  if (!res.set_cookie.empty()) httpd_resp_set_hdr(req, "Set-Cookie", res.set_cookie.c_str());
  return httpd_resp_send(req, res.body.data(), static_cast<ssize_t>(res.body.size()));
}

/** Announces the name on the network that just came up (Wi-Fi is the ESP32-C6 behind ESP-Hosted). */
void announce(void*, esp_event_base_t, int32_t, void*) {
  esp_netif_t* sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (sta == nullptr) return;
  mdns_netif_action(sta, static_cast<mdns_event_actions_t>(MDNS_EVENT_ENABLE_IP4 | MDNS_EVENT_ANNOUNCE_IP4));
}

void startMdns() {
  if (s_mdns) return;
  if (mdns_init() != ESP_OK) {
    ESP_LOGW(TAG, "mDNS didn't start; use the IP address");
    return;
  }
  mdns_hostname_set("plane-radar");
  mdns_instance_name_set("Plane Radar");
  mdns_service_add("Plane Radar", "_http", "_tcp", 80, nullptr, 0);
  s_mdns = true;
  esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, announce, nullptr);
  announce(nullptr, nullptr, 0, nullptr);  // already connected
}

}  // namespace

bool webServerStart(WebHandler handler) {
  if (s_server != nullptr) return true;
  s_handler = std::move(handler);
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  cfg.stack_size = 12 * 1024;
  cfg.core_id = 0;           // with the network stack, away from the UI
  cfg.task_priority = 3;     // below the ADS-B poller
  cfg.max_open_sockets = 4;  // a phone opens several; the least recently used is closed
  cfg.lru_purge_enable = true;
  cfg.recv_wait_timeout = 10;
  cfg.send_wait_timeout = 10;
  cfg.uri_match_fn = httpd_uri_match_wildcard;
  const esp_err_t err = httpd_start(&s_server, &cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "server didn't start: %s", esp_err_to_name(err));
    std::snprintf(s_stats.error, sizeof(s_stats.error), "Didn't start (%s)", esp_err_to_name(err));
    s_server = nullptr;
    return false;
  }
  s_stats.running = true;
  s_stats.error[0] = '\0';
  for (httpd_method_t m : {HTTP_GET, HTTP_POST}) {
    httpd_uri_t u = {};
    u.uri = "/*";
    u.method = m;
    u.handler = onRequest;
    httpd_register_uri_handler(s_server, &u);
  }
  startMdns();
  ESP_LOGI(TAG, "phone page on port 80 (plane-radar.local)");
  return true;
}

void webServerStop() {
  if (s_server == nullptr) return;
  httpd_stop(s_server);
  s_server = nullptr;
  s_stats.running = false;
}

WebStats webStats() { return s_stats; }

uint32_t random32() { return esp_random(); }

bool firmwareWriteBegin(std::string* error) {
  s_ota_part = esp_ota_get_next_update_partition(nullptr);
  if (s_ota_part == nullptr) {
    *error = "No update slot - flash this version over USB once";
    return false;
  }
  const esp_err_t err = esp_ota_begin(s_ota_part, OTA_WITH_SEQUENTIAL_WRITES, &s_ota);
  if (err != ESP_OK) {
    *error = std::string("Couldn't start (") + esp_err_to_name(err) + ")";
    return false;
  }
  return true;
}

bool firmwareWrite(const void* data, size_t len, std::string* error) {
  const esp_err_t err = esp_ota_write(s_ota, data, len);
  if (err != ESP_OK) {
    *error = std::string("Writing failed (") + esp_err_to_name(err) + ")";
    return false;
  }
  return true;
}

bool firmwareWriteEnd(bool commit, std::string* error) {
  if (!commit) {
    esp_ota_abort(s_ota);
    s_ota = 0;
    return true;
  }
  // Checks the image (chip, revision, checksum).
  esp_err_t err = esp_ota_end(s_ota);
  s_ota = 0;
  if (err == ESP_OK) err = esp_ota_set_boot_partition(s_ota_part);
  if (err != ESP_OK) {
    *error = err == ESP_ERR_OTA_VALIDATE_FAILED ? "The file is damaged or for another chip"
                                                : std::string("Install failed (") + esp_err_to_name(err) + ")";
    return false;
  }
  ESP_LOGI(TAG, "uploaded firmware installed; boots on restart");
  return true;
}

}  // namespace plat
