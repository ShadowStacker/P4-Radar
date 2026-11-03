// Wi-Fi station handling. On the P4 the radio is the on-board ESP32-C6;
// esp_wifi_remote + esp_hosted expose the regular esp_wifi API over SDIO.

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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
bool s_radio_ok = true;

// Every esp_wifi_* call is a request to the ESP32-C6 over SDIO, and can take
// seconds (up to the 5 s RPC timeout) when that link is busy or stuck. None
// is ever made while holding s_mu or from the UI's path: wifiStatus() only
// copies, and the "wifi" task below does the slow asking (signal strength,
// reconnecting, hotspot clients).
std::atomic<bool> s_retry_due{false};
std::atomic<bool> s_rejoin_due{false};
std::atomic<int> s_hotspot_clients{0};
TaskHandle_t s_wifi_task = nullptr;

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

/** Timer callback (esp_timer task, which also drives the screen's clock): only hands over. */
void retryNow(void*) {
  s_retry_due = true;
  if (s_wifi_task != nullptr) xTaskNotifyGive(s_wifi_task);
}

bool hotspotActive();

/** The "wifi" task: the slow requests to the radio, away from the screen and the feed. */
void wifiTask(void*) {
  uint32_t next_rssi_ms = 0, rssi_every_ms = 5000;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
    if (s_rejoin_due.exchange(false)) {
      bool rejoin = false;
      {
        std::lock_guard<std::mutex> lock(s_mu);
        if (s_want_connected) {
          s_status.state = WifiState::Connecting;
          rejoin = true;
        }
      }
      if (rejoin) {
        ESP_LOGW(TAG, "rejoining the network (internet not answering)");
        esp_wifi_disconnect();  // reported as ASSOC_LEAVE: no automatic retry
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_wifi_connect();
      }
    }
    if (s_retry_due.exchange(false)) {
      bool connect = false;
      {
        std::lock_guard<std::mutex> lock(s_mu);
        if (s_want_connected) {
          s_status.state = WifiState::Connecting;
          connect = true;
        }
      }
      if (connect) esp_wifi_connect();
    }
    const uint32_t now = millis();
    bool connected;
    {
      std::lock_guard<std::mutex> lock(s_mu);
      connected = s_status.state == WifiState::Connected;
    }
    if (connected && static_cast<int32_t>(now - next_rssi_ms) >= 0) {
      wifi_ap_record_t ap;
      const uint32_t t0 = millis();
      const bool ok = esp_wifi_sta_get_ap_info(&ap) == ESP_OK;
      const uint32_t took = millis() - t0;
      // A slow or failed answer means the link is struggling: ask less often.
      rssi_every_ms = ok && took < 500 ? 5000 : std::min<uint32_t>(rssi_every_ms * 2, 60000);
      if (took >= 500) ESP_LOGW(TAG, "signal check took %u ms", static_cast<unsigned>(took));
      next_rssi_ms = millis() + rssi_every_ms;
      if (ok) {
        std::lock_guard<std::mutex> lock(s_mu);
        s_status.rssi = ap.rssi;
      }
    }
    if (hotspotActive()) {
      wifi_sta_list_t list = {};
      s_hotspot_clients = esp_wifi_ap_get_sta_list(&list) == ESP_OK ? list.num : 0;
    } else {
      s_hotspot_clients = 0;
    }
  }
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

  // Internet time for the clock face and alarms. SNTP keeps retrying in the
  // background and syncs as soon as Wi-Fi is up (then hourly).
  esp_sntp_config_t sntp = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
  if (esp_netif_sntp_init(&sntp) != ESP_OK) ESP_LOGW(TAG, "SNTP init failed; the clock won't be set");

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
    // Mains powered: no modem sleep. Asleep between beacons, the radio is
    // slow to notice (or misses) phones connecting to the settings page.
    if (err == ESP_OK) esp_wifi_set_ps(WIFI_PS_NONE);
  }
  if (err == ESP_OK) xTaskCreatePinnedToCore(wifiTask, "wifi", 4096, nullptr, 3, &s_wifi_task, 0);
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

namespace {
esp_netif_t* s_ap_netif = nullptr;
std::atomic<bool> s_ap_on{false};
bool hotspotActive() { return s_ap_on; }
}  // namespace

bool hotspotStart(const char* ssid, const char* pass) {
  if (!s_radio_ok) return false;
  if (s_ap_netif == nullptr) s_ap_netif = esp_netif_create_default_wifi_ap();  // 192.168.4.1, with a DHCP server
  wifi_config_t ap = {};
  std::snprintf(reinterpret_cast<char*>(ap.ap.ssid), sizeof(ap.ap.ssid), "%s", ssid);
  ap.ap.ssid_len = static_cast<uint8_t>(std::strlen(reinterpret_cast<char*>(ap.ap.ssid)));
  std::snprintf(reinterpret_cast<char*>(ap.ap.password), sizeof(ap.ap.password), "%s", pass);
  ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
  ap.ap.max_connection = 4;
  ap.ap.channel = 0;  // the radio follows the normal network's channel
  esp_err_t err = esp_wifi_set_mode(WIFI_MODE_APSTA);
  if (err == ESP_OK) err = esp_wifi_set_config(WIFI_IF_AP, &ap);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "hotspot: %s", esp_err_to_name(err));
    esp_wifi_set_mode(WIFI_MODE_STA);
    return false;
  }
  s_ap_on = true;
  ESP_LOGI(TAG, "hotspot %s on", ssid);
  return true;
}

void hotspotStop() {
  if (!s_ap_on) return;
  esp_wifi_set_mode(WIFI_MODE_STA);
  s_ap_on = false;
  ESP_LOGI(TAG, "hotspot off");
}

bool hotspotOn() { return s_ap_on; }

int hotspotClients() { return s_ap_on ? s_hotspot_clients.load() : 0; }

const char* hotspotIp() { return "192.168.4.1"; }

WifiStatus wifiStatus() {
  std::lock_guard<std::mutex> lock(s_mu);  // never held across a radio request
  return s_status;
}

void wifiConnect(const char* ssid, const char* pass) {
  {
    std::lock_guard<std::mutex> lock(s_mu);
    if (!s_radio_ok) return;
    esp_timer_stop(s_retry_timer);
    s_retry_due = false;
    s_want_connected = false;
    s_status = WifiStatus{};
  }
  esp_wifi_disconnect();
  if (ssid == nullptr || ssid[0] == '\0') return;

  wifi_config_t wc = {};
  std::snprintf(reinterpret_cast<char*>(wc.sta.ssid), sizeof(wc.sta.ssid), "%s", ssid);
  std::snprintf(reinterpret_cast<char*>(wc.sta.password), sizeof(wc.sta.password), "%s", pass ? pass : "");
  wc.sta.threshold.authmode = (pass && pass[0]) ? WIFI_AUTH_WPA_PSK : WIFI_AUTH_OPEN;
  wc.sta.pmf_cfg.capable = true;
  wc.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
  wc.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
  esp_wifi_set_config(WIFI_IF_STA, &wc);
  {
    std::lock_guard<std::mutex> lock(s_mu);
    s_want_connected = true;
    s_retry_delay_ms = 2000;
    s_status.state = WifiState::Connecting;
    std::snprintf(s_status.ssid, sizeof(s_status.ssid), "%s", ssid);
  }
  esp_wifi_connect();
}

void wifiReconnect() {
  s_rejoin_due = true;
  if (s_wifi_task != nullptr) xTaskNotifyGive(s_wifi_task);
}

void wifiScanStart() {
  std::unique_lock<std::mutex> lock(s_mu);
  if (!s_radio_ok) {
    s_scan_results.clear();
    s_scan_done = true;
    return;
  }
  if (s_scan_running) return;
  s_scan_done = false;
  s_scan_running = true;
  lock.unlock();
  wifi_scan_config_t sc = {};
  sc.show_hidden = false;
  const bool started = esp_wifi_scan_start(&sc, false) == ESP_OK;
  lock.lock();
  if (!started) {
    s_scan_running = false;
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
