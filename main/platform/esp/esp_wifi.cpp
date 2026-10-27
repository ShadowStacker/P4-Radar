// Wi-Fi station handling. On the P4 the radio is the on-board ESP32-C6;
// esp_wifi_remote + esp_hosted expose the regular esp_wifi API over SDIO.

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <mutex>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "platform/esp/esp_platform.h"
#include "platform/platform.h"

namespace plat {

namespace {

const char* TAG = "wifi";

std::mutex s_mu;
WifiStatus s_status;
bool s_want_connected = false;
uint32_t s_retry_delay_ms = 2000;
esp_timer_handle_t s_retry_timer = nullptr;
esp_netif_t* s_netif = nullptr;
uint32_t s_rssi_checked_ms = 0;
bool s_radio_ok = true;

bool s_scan_running = false;
bool s_scan_done = false;
std::vector<WifiNetwork> s_scan_results;

const char* reasonText(uint8_t reason) {
  switch (reason) {
    case WIFI_REASON_NO_AP_FOUND: return "Network not found";
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_MIC_FAILURE: return "Wrong password?";
    case WIFI_REASON_ASSOC_FAIL:
    case WIFI_REASON_AUTH_EXPIRE: return "Router refused";
    case WIFI_REASON_BEACON_TIMEOUT: return "Signal lost";
    default: return "Disconnected";
  }
}

void retryNow(void*) {
  std::lock_guard<std::mutex> lock(s_mu);
  if (!s_want_connected) return;
  s_status.state = WifiState::Connecting;
  esp_wifi_connect();
}

void onWifiEvent(void*, esp_event_base_t base, int32_t id, void* data) {
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
    const auto* ev = static_cast<wifi_event_sta_disconnected_t*>(data);
    // Our own esp_wifi_disconnect() (switching networks) reports ASSOC_LEAVE.
    if (ev->reason == WIFI_REASON_ASSOC_LEAVE) return;
    std::lock_guard<std::mutex> lock(s_mu);
    if (!s_want_connected) {
      s_status.state = WifiState::Idle;
      return;
    }
    s_status.state = WifiState::Failed;
    s_status.ip[0] = '\0';
    std::snprintf(s_status.reason, sizeof(s_status.reason), "%s", reasonText(ev->reason));
    ESP_LOGW(TAG, "disconnected (reason %d), retry in %u ms", ev->reason, static_cast<unsigned>(s_retry_delay_ms));
    esp_timer_stop(s_retry_timer);
    esp_timer_start_once(s_retry_timer, static_cast<uint64_t>(s_retry_delay_ms) * 1000);
    s_retry_delay_ms = std::min<uint32_t>(s_retry_delay_ms * 2, 30000);
  } else if (base == WIFI_EVENT && id == WIFI_EVENT_SCAN_DONE) {
    uint16_t n = 0;
    esp_wifi_scan_get_ap_num(&n);
    std::vector<wifi_ap_record_t> recs(n);
    if (n > 0) esp_wifi_scan_get_ap_records(&n, recs.data());
    std::vector<WifiNetwork> nets;
    for (uint16_t i = 0; i < n; ++i) {
      const char* ssid = reinterpret_cast<const char*>(recs[i].ssid);
      if (ssid[0] == '\0') continue;  // hidden
      auto it = std::find_if(nets.begin(), nets.end(), [ssid](const WifiNetwork& w) { return w.ssid == ssid; });
      if (it != nets.end()) {
        it->rssi = std::max(it->rssi, static_cast<int>(recs[i].rssi));
        continue;
      }
      nets.push_back({ssid, recs[i].rssi, recs[i].authmode != WIFI_AUTH_OPEN});
    }
    std::sort(nets.begin(), nets.end(), [](const WifiNetwork& a, const WifiNetwork& b) { return a.rssi > b.rssi; });
    std::lock_guard<std::mutex> lock(s_mu);
    s_scan_results = std::move(nets);
    s_scan_running = false;
    s_scan_done = true;
  } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
    const auto* ev = static_cast<ip_event_got_ip_t*>(data);
    std::lock_guard<std::mutex> lock(s_mu);
    s_status.state = WifiState::Connected;
    s_status.reason[0] = '\0';
    std::snprintf(s_status.ip, sizeof(s_status.ip), IPSTR, IP2STR(&ev->ip_info.ip));
    s_retry_delay_ms = 2000;
    s_rssi_checked_ms = 0;
    ESP_LOGI(TAG, "connected, ip %s", s_status.ip);
  }
}

}  // namespace

namespace esp {

bool wifiStart() {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  s_netif = esp_netif_create_default_wifi_sta();
  esp_netif_set_hostname(s_netif, "plane-radar");

  esp_timer_create_args_t targs = {};
  targs.callback = retryNow;
  targs.name = "wifi_retry";
  ESP_ERROR_CHECK(esp_timer_create(&targs, &s_retry_timer));

  // The radio is the ESP32-C6 co-processor. If it doesn't answer (missing or
  // incompatible ESP-Hosted firmware), keep running without Wi-Fi so the UI
  // can say so, instead of rebooting in a loop.
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_err_t err = esp_wifi_init(&cfg);
  if (err == ESP_OK) {
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, onWifiEvent, nullptr);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, onWifiEvent, nullptr);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);  // credentials live in our own NVS
    esp_wifi_set_mode(WIFI_MODE_STA);
    err = esp_wifi_start();
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Wi-Fi co-processor not available: %s", esp_err_to_name(err));
    std::lock_guard<std::mutex> lock(s_mu);
    s_radio_ok = false;
    s_status.state = WifiState::Failed;
    std::snprintf(s_status.reason, sizeof(s_status.reason), "Wi-Fi module not responding");
    return false;
  }
  return true;
}

}  // namespace esp

WifiStatus wifiStatus() {
  std::lock_guard<std::mutex> lock(s_mu);
  if (s_status.state == WifiState::Connected && millis() - s_rssi_checked_ms > 5000) {
    s_rssi_checked_ms = millis();
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) s_status.rssi = ap.rssi;
  }
  return s_status;
}

void wifiConnect(const char* ssid, const char* pass) {
  std::lock_guard<std::mutex> lock(s_mu);
  if (!s_radio_ok) return;
  esp_timer_stop(s_retry_timer);
  s_want_connected = false;
  esp_wifi_disconnect();
  s_status = WifiStatus{};
  if (ssid == nullptr || ssid[0] == '\0') return;

  wifi_config_t wc = {};
  std::snprintf(reinterpret_cast<char*>(wc.sta.ssid), sizeof(wc.sta.ssid), "%s", ssid);
  std::snprintf(reinterpret_cast<char*>(wc.sta.password), sizeof(wc.sta.password), "%s", pass ? pass : "");
  wc.sta.threshold.authmode = (pass && pass[0]) ? WIFI_AUTH_WPA_PSK : WIFI_AUTH_OPEN;
  wc.sta.pmf_cfg.capable = true;
  wc.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
  wc.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
  esp_wifi_set_config(WIFI_IF_STA, &wc);

  s_want_connected = true;
  s_retry_delay_ms = 2000;
  s_status.state = WifiState::Connecting;
  std::snprintf(s_status.ssid, sizeof(s_status.ssid), "%s", ssid);
  esp_wifi_connect();
}

void wifiScanStart() {
  std::lock_guard<std::mutex> lock(s_mu);
  if (!s_radio_ok) {
    s_scan_results.clear();
    s_scan_done = true;
    return;
  }
  if (s_scan_running) return;
  s_scan_done = false;
  wifi_scan_config_t sc = {};
  sc.show_hidden = false;
  if (esp_wifi_scan_start(&sc, false) == ESP_OK) {
    s_scan_running = true;
  } else {
    // Typically "busy connecting"; report an empty result so the UI recovers.
    ESP_LOGW(TAG, "scan could not start");
    s_scan_results.clear();
    s_scan_done = true;
  }
}

bool wifiScanResults(std::vector<WifiNetwork>* out) {
  std::lock_guard<std::mutex> lock(s_mu);
  if (!s_scan_done) return false;
  *out = s_scan_results;
  s_scan_done = false;
  return true;
}

}  // namespace plat
