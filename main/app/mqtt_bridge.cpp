#include "app/mqtt_bridge.h"

#include <cmath>
#include <cstdio>
#include <mutex>

#include "app/app.h"
#include "core/mqtt.h"
#include "platform/platform.h"

namespace app::mqtt {

namespace {

constexpr uint32_t kMinStateGapMs = 2000;     // at most this often
constexpr uint32_t kMaxStateGapMs = 60000;    // and at least this often (so "last seen" stays fresh)

std::mutex s_mu;  // guards s_status_text
std::string s_status_text = "Off";

std::string s_prefix;                 // the prefix in use
uint32_t s_discovered_connects = 0;   // plat::mqttConnects() when discovery was last sent
std::string s_last_state;
uint32_t s_last_state_ms = 0;
bool s_started = false;

void setStatus(const std::string& text) {
  std::lock_guard<std::mutex> lock(s_mu);
  s_status_text = text;
}

}  // namespace

void service() {
  const core::Settings& st = settings();
  const bool on = st.mqtt_enabled && st.mqtt_host[0] != '\0';
  if (!on) {
    if (s_started) {
      plat::mqttStart({});
      s_started = false;
      s_discovered_connects = 0;
    }
    setStatus(st.mqtt_enabled ? "No broker address" : "Off");
    return;
  }
  s_prefix = core::mqtt::cleanPrefix(st.mqtt_topic);
  plat::MqttConfig cfg;
  cfg.host = st.mqtt_host;
  cfg.port = st.mqtt_port ? st.mqtt_port : 1883;
  cfg.user = st.mqtt_user;
  cfg.pass = st.mqtt_pass;
  cfg.will_topic = core::mqtt::statusTopic(s_prefix);
  plat::mqttStart(cfg);  // (does nothing when it's already running with this)
  s_started = true;
  const bool connected = plat::mqttConnected();
  setStatus((connected ? "Connected to " : "Connecting to ") + cfg.host);
  if (!connected) return;
  const uint32_t connects = plat::mqttConnects();
  if (connects != s_discovered_connects) {
    // Just (re)connected: the retained messages again (the broker may have restarted).
    s_discovered_connects = connects;
    if (st.mqtt_discovery) {
      for (const core::mqtt::Message& m :
           core::mqtt::discovery(s_prefix, plat::deviceId(), plat::firmwareVersion(), unitSuffix())) {
        plat::mqttPublish(m.topic, m.payload, m.retain);
      }
    }
    if (!s_last_state.empty()) plat::mqttPublish(core::mqtt::stateTopic(s_prefix), s_last_state, true);
  }
}

void traffic(const std::vector<core::Aircraft>& shown, const core::UpdateEvents& ev,
             const std::vector<core::logbook::Entry>& firsts, const char* source) {
  const core::Settings& st = settings();
  if (!st.mqtt_enabled || !plat::mqttConnected()) return;

  core::mqtt::State state;
  state.count = static_cast<int>(shown.size());
  state.source = source;
  state.units = unitSuffix();
  const core::LatLon home{st.home_lat, st.home_lon};
  double best = 1e18;
  for (const core::Aircraft& a : shown) {
    if (a.emergency) ++state.emergencies;
    const double km = core::distanceKm(home, {a.lat, a.lon});
    if (km < best) {
      best = km;
      core::mqtt::Nearest& n = state.nearest;
      n.valid = true;
      n.name = core::displayName(a);
      n.hex = a.hex;
      n.type = a.type;
      n.distance = std::round(kmToUnits(km) * 10) / 10;
      n.alt_ft = a.alt_ft;
      n.alt_valid = a.alt_valid;
      n.on_ground = a.on_ground;
    }
  }
  const std::string json = core::mqtt::stateJson(state);
  const uint32_t now = plat::millis();
  if ((json != s_last_state && now - s_last_state_ms >= kMinStateGapMs) || now - s_last_state_ms >= kMaxStateGapMs ||
      s_last_state_ms == 0) {
    plat::mqttPublish(core::mqtt::stateTopic(s_prefix), json, true);
    s_last_state = json;
    s_last_state_ms = now;
  }

  // Alerts: one message each, whatever the sound settings.
  const std::string topic = core::mqtt::eventTopic(s_prefix);
  for (const std::string& n : ev.favorites_near) plat::mqttPublish(topic, core::mqtt::eventJson("favorite_near", n, "", ""), false);
  for (const std::string& n : ev.favorites_seen) plat::mqttPublish(topic, core::mqtt::eventJson("favorite_seen", n, "", ""), false);
  for (const core::WatchHit& w : ev.watch_hits) {
    char dist[24];
    formatDistance(w.km, dist, sizeof(dist));
    plat::mqttPublish(topic, core::mqtt::eventJson("watchlist", w.name, w.type, w.reason + ", " + dist), false);
  }
  if (ev.new_emergencies > 0) {
    for (const core::Aircraft& a : shown) {
      if (a.emergency) {
        plat::mqttPublish(topic, core::mqtt::eventJson("emergency", core::displayName(a), a.type, a.squawk), false);
      }
    }
  }
  if (st.first_time_alert) {
    for (const core::logbook::Entry& e : firsts) {
      plat::mqttPublish(topic, core::mqtt::eventJson("first_time", e.label, e.type, ""), false);
    }
  }
}

std::string status() {
  std::lock_guard<std::mutex> lock(s_mu);
  return s_status_text;
}

}  // namespace app::mqtt
