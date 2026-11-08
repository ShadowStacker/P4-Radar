#include "core/mqtt.h"

#include <cmath>
#include <cstdio>

#include "core/web.h"

namespace core::mqtt {

namespace {

using web::jsonString;

std::string num(double v, int decimals) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
  return buf;
}

}  // namespace

std::string cleanPrefix(const char* setting) {
  std::string s;
  for (const char* p = setting; p != nullptr && *p != '\0'; ++p) {
    const char c = *p;
    if (c == '+' || c == '#' || c == ' ' || c == '\t' || static_cast<unsigned char>(c) < 32) continue;
    s.push_back(c);
  }
  while (!s.empty() && s.front() == '/') s.erase(s.begin());
  while (!s.empty() && s.back() == '/') s.pop_back();
  return s.empty() ? "plane_radar" : s;
}

std::string statusTopic(const std::string& prefix) { return prefix + "/status"; }
std::string stateTopic(const std::string& prefix) { return prefix + "/state"; }
std::string eventTopic(const std::string& prefix) { return prefix + "/event"; }

std::string stateJson(const State& s) {
  std::string out = "{\"count\":" + std::to_string(s.count) + ",\"emergencies\":" + std::to_string(s.emergencies) +
                    ",\"emergency\":" + (s.emergencies > 0 ? "true" : "false") + ",\"source\":" + jsonString(s.source) +
                    ",\"units\":" + jsonString(s.units) + ",\"nearest\":";
  const Nearest& n = s.nearest;
  if (!n.valid) {
    out += "null";
  } else {
    out += "{\"name\":" + jsonString(n.name) + ",\"hex\":" + jsonString(n.hex) + ",\"type\":" + jsonString(n.type) +
           ",\"distance\":" + num(n.distance, 1) + ",\"altitude_ft\":" +
           (n.on_ground ? std::string("0") : n.alt_valid ? std::to_string(n.alt_ft) : std::string("null")) +
           ",\"on_ground\":" + (n.on_ground ? "true" : "false") + "}";
  }
  out += "}";
  return out;
}

std::string eventJson(const char* kind, const std::string& name, const std::string& type, const std::string& detail) {
  return "{\"event_type\":" + jsonString(kind) + ",\"name\":" + jsonString(name) + ",\"type\":" + jsonString(type) +
         ",\"detail\":" + jsonString(detail) + "}";
}

std::vector<Message> discovery(const std::string& prefix, const std::string& device_id, const char* fw_version,
                               const std::string& units) {
  const std::string device = "\"device\":{\"identifiers\":[" + jsonString(device_id) +
                             "],\"name\":\"Plane Radar\",\"manufacturer\":\"Plane Radar P4\",\"model\":\"ESP32-P4 "
                             "radar\",\"sw_version\":" +
                             jsonString(fw_version ? fw_version : "") + "}";
  const std::string common = "\"availability_topic\":" + jsonString(statusTopic(prefix)) + "," + device;
  const std::string state = jsonString(stateTopic(prefix));
  std::vector<Message> out;
  auto add = [&](const char* component, const char* object, const std::string& body) {
    out.push_back({"homeassistant/" + std::string(component) + "/" + device_id + "/" + object + "/config",
                   "{" + body + "," + common + "}", true});
  };
  add("sensor", "count",
      "\"name\":\"Aircraft nearby\",\"unique_id\":" + jsonString(device_id + "_count") + ",\"state_topic\":" + state +
          ",\"value_template\":\"{{ value_json.count }}\",\"unit_of_measurement\":\"aircraft\","
          "\"state_class\":\"measurement\",\"icon\":\"mdi:airplane\"");
  add("sensor", "nearest",
      "\"name\":\"Nearest aircraft\",\"unique_id\":" + jsonString(device_id + "_nearest") + ",\"state_topic\":" +
          state +
          ",\"value_template\":\"{{ value_json.nearest.name if value_json.nearest else 'none' }}\","
          "\"json_attributes_topic\":" +
          state + ",\"json_attributes_template\":\"{{ (value_json.nearest or {}) | tojson }}\",\"icon\":\"mdi:airplane-marker\"");
  add("sensor", "nearest_distance",
      "\"name\":\"Nearest aircraft distance\",\"unique_id\":" + jsonString(device_id + "_nearest_distance") +
          ",\"state_topic\":" + state +
          ",\"value_template\":\"{{ value_json.nearest.distance if value_json.nearest else none }}\","
          "\"unit_of_measurement\":" +
          jsonString(units) + ",\"state_class\":\"measurement\",\"icon\":\"mdi:map-marker-distance\"");
  add("sensor", "nearest_altitude",
      "\"name\":\"Nearest aircraft altitude\",\"unique_id\":" + jsonString(device_id + "_nearest_altitude") +
          ",\"state_topic\":" + state +
          ",\"value_template\":\"{{ value_json.nearest.altitude_ft if value_json.nearest else none }}\","
          "\"unit_of_measurement\":\"ft\",\"state_class\":\"measurement\",\"icon\":\"mdi:altimeter\"");
  add("binary_sensor", "emergency",
      "\"name\":\"Aircraft emergency\",\"unique_id\":" + jsonString(device_id + "_emergency") + ",\"state_topic\":" +
          state +
          ",\"value_template\":\"{{ 'ON' if value_json.emergency else 'OFF' }}\",\"device_class\":\"problem\","
          "\"icon\":\"mdi:alert\"");
  add("event", "alert",
      "\"name\":\"Aircraft alert\",\"unique_id\":" + jsonString(device_id + "_alert") +
          ",\"state_topic\":" + jsonString(eventTopic(prefix)) +
          ",\"event_types\":[\"favorite_seen\",\"favorite_near\",\"watchlist\",\"emergency\",\"first_time\"],"
          "\"icon\":\"mdi:airplane-alert\"");
  return out;
}

}  // namespace core::mqtt
