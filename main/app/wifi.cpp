// Saved Wi-Fi networks: the one to use first plus up to four more. When the
// current one can't connect for a while, the others are tried in turn, and
// whichever connects becomes the one to use first.

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "app/app.h"
#include "app/wifi_internal.h"
#include "platform/platform.h"

namespace app {

namespace {

constexpr uint32_t kTryEachMs = 20000;  // how long each network gets before the next is tried

size_t s_target = 0;         // index into savedWifi() being connected to
uint32_t s_attempt_ms = 0;   // when the current attempt started (or the connection was last seen)

void setAll(const std::vector<core::SavedWifi>& v) {
  core::Settings& st = settings();
  std::snprintf(st.wifi_ssid, sizeof(st.wifi_ssid), "%s", v.empty() ? "" : v[0].ssid);
  std::snprintf(st.wifi_pass, sizeof(st.wifi_pass), "%s", v.empty() ? "" : v[0].pass);
  for (int i = 0; i < core::kMaxSavedWifi; ++i) {
    st.wifi_saved[i] = i + 1 < static_cast<int>(v.size()) ? v[i + 1] : core::SavedWifi{};
  }
}

void eraseSsid(std::vector<core::SavedWifi>* v, const char* ssid) {
  v->erase(std::remove_if(v->begin(), v->end(),
                          [ssid](const core::SavedWifi& w) { return std::strcmp(w.ssid, ssid) == 0; }),
           v->end());
}

}  // namespace

std::vector<core::SavedWifi> savedWifi() {
  const core::Settings& st = settings();
  std::vector<core::SavedWifi> v;
  if (st.wifi_ssid[0] != '\0') {
    core::SavedWifi w;
    std::snprintf(w.ssid, sizeof(w.ssid), "%s", st.wifi_ssid);
    std::snprintf(w.pass, sizeof(w.pass), "%s", st.wifi_pass);
    v.push_back(w);
  }
  for (const core::SavedWifi& w : st.wifi_saved) {
    bool dup = false;
    for (const core::SavedWifi& o : v) dup |= std::strcmp(o.ssid, w.ssid) == 0;
    if (w.ssid[0] != '\0' && !dup) v.push_back(w);
  }
  return v;
}

bool isSavedWifi(const char* ssid, std::string* pass) {
  for (const core::SavedWifi& w : savedWifi()) {
    if (std::strcmp(w.ssid, ssid) == 0) {
      if (pass) *pass = w.pass;
      return true;
    }
  }
  return false;
}

void useWifi(const char* ssid, const char* pass) {
  std::vector<core::SavedWifi> v = savedWifi();
  eraseSsid(&v, ssid);
  core::SavedWifi w;
  std::snprintf(w.ssid, sizeof(w.ssid), "%s", ssid);
  std::snprintf(w.pass, sizeof(w.pass), "%s", pass);
  v.insert(v.begin(), w);
  if (v.size() > 1 + core::kMaxSavedWifi) v.resize(1 + core::kMaxSavedWifi);
  setAll(v);
  settingsChanged();
  settingsFlush();
  plat::wifiConnect(ssid, pass);
  s_target = 0;
  s_attempt_ms = plat::millis();
}

void forgetWifi(const char* ssid) {
  std::vector<core::SavedWifi> v = savedWifi();
  const bool was_first = !v.empty() && std::strcmp(v[0].ssid, ssid) == 0;
  eraseSsid(&v, ssid);
  setAll(v);
  settingsChanged();
  settingsFlush();
  const plat::WifiStatus ws = plat::wifiStatus();
  if (was_first || std::strcmp(ws.ssid, ssid) == 0) {
    if (v.empty()) plat::wifiConnect("", "");
    else plat::wifiConnect(v[0].ssid, v[0].pass);
    s_target = 0;
    s_attempt_ms = plat::millis();
  }
}

void serviceWifi(uint32_t now_ms) {
  const std::vector<core::SavedWifi> v = savedWifi();
  const plat::WifiStatus ws = plat::wifiStatus();
  if (ws.state == plat::WifiState::Connected) {
    // Connected to another saved network than the first: it becomes the first.
    if (!v.empty() && std::strcmp(v[0].ssid, ws.ssid) != 0) {
      for (size_t i = 1; i < v.size(); ++i) {
        if (std::strcmp(v[i].ssid, ws.ssid) == 0) {
          std::vector<core::SavedWifi> order = v;
          std::rotate(order.begin(), order.begin() + static_cast<long>(i), order.begin() + static_cast<long>(i) + 1);
          setAll(order);
          settingsChanged();
          std::printf("wifi: %s is now the network to use first\n", ws.ssid);
          break;
        }
      }
    }
    s_target = 0;
    s_attempt_ms = now_ms;
    return;
  }
  if (v.size() < 2) {
    s_attempt_ms = now_ms;  // one network: the driver keeps retrying it
    return;
  }
  if (s_attempt_ms == 0) s_attempt_ms = now_ms;
  if (now_ms - s_attempt_ms < kTryEachMs) return;
  s_target = (s_target + 1) % v.size();
  s_attempt_ms = now_ms;
  std::printf("wifi: no connection for %u s, trying %s\n", static_cast<unsigned>(kTryEachMs / 1000),
              v[s_target].ssid);
  plat::wifiConnect(v[s_target].ssid, v[s_target].pass);
}

}  // namespace app
