#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace core::mqtt {

/**
 * What the radar sends to an MQTT broker (Home Assistant, Node-RED...):
 *
 *   <prefix>/status  "online" / "offline" (retained; "offline" is the last will)
 *   <prefix>/state   the traffic now, JSON (retained), at most every few seconds
 *   <prefix>/event   one JSON message per alert: a favorite seen or nearby, a
 *                    watchlist aircraft, an emergency, a first-time visitor
 *
 * and, with discovery on, Home Assistant sensors for them under
 * homeassistant/... (retained), so they appear without any YAML.
 */

struct Nearest {
  bool valid = false;
  std::string name;  // callsign, registration or "Radar track"
  std::string hex;
  std::string type;  // ICAO type, may be empty
  double distance = 0;  // in the radar's units
  int32_t alt_ft = 0;
  bool alt_valid = false;
  bool on_ground = false;
};

struct State {
  int count = 0;        // aircraft shown
  Nearest nearest;      // to home
  int emergencies = 0;  // aircraft squawking an emergency now
  std::string source;   // "adsb.fi", "your receiver"...
  std::string units;    // "km", "mi" or "nm"
};

struct Message {
  std::string topic;
  std::string payload;
  bool retain = false;
};

/** A topic prefix that is safe to use: no spaces, wildcards or slashes at either end ("plane_radar" if empty). */
std::string cleanPrefix(const char* setting);

std::string statusTopic(const std::string& prefix);
std::string stateTopic(const std::string& prefix);
std::string eventTopic(const std::string& prefix);

std::string stateJson(const State& s);

/** One alert. `kind`: favorite_seen, favorite_near, watchlist, emergency, first_time. */
std::string eventJson(const char* kind, const std::string& name, const std::string& type, const std::string& detail);

/**
 * Home Assistant discovery messages for the sensors (one device, its
 * entities). `device_id` must be unique per radar (e.g. from the MAC).
 */
std::vector<Message> discovery(const std::string& prefix, const std::string& device_id, const char* fw_version,
                               const std::string& units);

}  // namespace core::mqtt
