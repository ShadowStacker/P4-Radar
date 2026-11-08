// MQTT for Home Assistant: ESP-IDF's client, started only when it's switched on.

#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>

#include "esp_log.h"
#include "esp_mac.h"
#include "mqtt_client.h"
#include "platform/platform.h"

namespace plat {

namespace {

const char* TAG = "mqtt";
std::mutex s_mu;  // guards s_client and s_cfg
esp_mqtt_client_handle_t s_client = nullptr;
MqttConfig s_cfg;
std::string s_uri;
std::atomic<bool> s_connected{false};
std::atomic<uint32_t> s_connects{0};

void onEvent(void*, esp_event_base_t, int32_t id, void* data) {
  auto* ev = static_cast<esp_mqtt_event_handle_t>(data);
  switch (static_cast<esp_mqtt_event_id_t>(id)) {
    case MQTT_EVENT_CONNECTED:
      ESP_LOGI(TAG, "connected to %s", s_uri.c_str());
      // The last will says "offline"; say "online" now.
      esp_mqtt_client_enqueue(ev->client, s_cfg.will_topic.c_str(), "online", 0, 0, 1, true);
      s_connected = true;
      ++s_connects;
      break;
    case MQTT_EVENT_DISCONNECTED:
      if (s_connected.exchange(false)) ESP_LOGW(TAG, "disconnected (it reconnects by itself)");
      break;
    case MQTT_EVENT_ERROR:
      if (ev->error_handle != nullptr && ev->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
        ESP_LOGW(TAG, "broker refused the connection (user name or password?)");
      }
      break;
    default: break;
  }
}

}  // namespace

void mqttStart(const MqttConfig& cfg) {
  std::lock_guard<std::mutex> lock(s_mu);
  if (s_client != nullptr) {
    if (cfg.host == s_cfg.host && cfg.port == s_cfg.port && cfg.user == s_cfg.user && cfg.pass == s_cfg.pass &&
        cfg.will_topic == s_cfg.will_topic) {
      return;  // already running with this
    }
    esp_mqtt_client_stop(s_client);
    esp_mqtt_client_destroy(s_client);
    s_client = nullptr;
    s_connected = false;
  }
  s_cfg = cfg;
  if (cfg.host.empty()) return;
  s_uri = "mqtt://" + cfg.host + ":" + std::to_string(cfg.port);
  esp_mqtt_client_config_t c = {};
  c.broker.address.uri = s_uri.c_str();
  if (!s_cfg.user.empty()) c.credentials.username = s_cfg.user.c_str();
  if (!s_cfg.pass.empty()) c.credentials.authentication.password = s_cfg.pass.c_str();
  c.credentials.client_id = nullptr;  // the library makes one from the MAC
  c.session.last_will.topic = s_cfg.will_topic.c_str();
  c.session.last_will.msg = "offline";
  c.session.last_will.retain = 1;
  c.session.keepalive = 60;
  c.network.reconnect_timeout_ms = 10000;
  c.network.timeout_ms = 5000;
  c.task.priority = 2;  // below the feed and the screen
  c.outbox.limit = 32 * 1024;  // messages waiting while disconnected (oldest dropped beyond)
  s_client = esp_mqtt_client_init(&c);
  if (s_client == nullptr) {
    ESP_LOGE(TAG, "can't create the client");
    return;
  }
  esp_mqtt_client_register_event(s_client, MQTT_EVENT_ANY, onEvent, nullptr);
  if (esp_mqtt_client_start(s_client) != ESP_OK) ESP_LOGE(TAG, "can't start the client");
}

bool mqttConnected() { return s_connected.load(); }

uint32_t mqttConnects() { return s_connects.load(); }

void mqttPublish(const std::string& topic, const std::string& payload, bool retain) {
  std::lock_guard<std::mutex> lock(s_mu);
  if (s_client == nullptr || !s_connected) return;
  esp_mqtt_client_enqueue(s_client, topic.c_str(), payload.data(), static_cast<int>(payload.size()), 0,
                          retain ? 1 : 0, true);
}

std::string deviceId() {
  uint8_t mac[6] = {};
  esp_efuse_mac_get_default(mac);
  char buf[32];
  std::snprintf(buf, sizeof(buf), "plane_radar_%02x%02x%02x", mac[3], mac[4], mac[5]);
  return buf;
}

}  // namespace plat
