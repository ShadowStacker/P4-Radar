// The settings page for phones: routes the web server's requests (they run
// in its own task) to the app. Anything that touches the settings runs on
// the UI task through runOnUi(), like a tap on the screen would.

#include "app/web.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "app/app.h"
#include "app/maps.h"
#include "core/daylog.h"
#include "core/fwcheck.h"
#include "core/settings_text.h"
#include "core/web.h"
#include "platform/platform.h"

namespace app::web {

namespace {

using core::web::jsonString;

std::mutex s_mu;  // guards the copies below (the server's task reads them)
std::string s_pin, s_token;
core::web::LoginGuard s_guard;
bool s_running = false;           // UI task
uint32_t s_restart_at_ms = 0;     // UI task: restart asked for from the page
std::vector<plat::WifiNetwork> s_scan;  // last scan, for the page
bool s_scan_done = false;

plat::WebResponse json(const std::string& body, int status = 200) {
  plat::WebResponse r;
  r.status = status;
  r.body = body;
  return r;
}

plat::WebResponse error(const std::string& what, int status = 400) {
  return json("{\"ok\":false,\"error\":" + jsonString(what) + "}", status);
}

plat::WebResponse ok(const std::string& extra = "") { return json("{\"ok\":true" + extra + "}"); }

plat::WebResponse busy() { return error("The radar is busy - try again", 503); }

std::string randomDigits(int n) {
  std::string s;
  for (int i = 0; i < n; ++i) s.push_back(static_cast<char>('0' + plat::random32() % 10));
  return s;
}

std::string randomHex(int n) {
  static const char kHex[] = "0123456789abcdef";
  std::string s;
  for (int i = 0; i < n; ++i) s.push_back(kHex[plat::random32() % 16]);
  return s;
}

bool loggedIn(const plat::WebRequest& r) {
  const std::string c = core::web::cookie(r.cookie, "radar");
  std::lock_guard<std::mutex> lock(s_mu);
  return !s_token.empty() && c == s_token;
}

plat::WebResponse login(const plat::WebRequest& r) {
  const std::string pin = core::web::parseForm(r.body)["pin"];
  std::lock_guard<std::mutex> lock(s_mu);
  const uint32_t now = plat::millis();
  if (const int wait = s_guard.lockedFor(now)) {
    return error("Too many wrong PINs - try again in " + std::to_string(wait) + " s", 429);
  }
  if (s_pin.empty() || pin != s_pin) {
    s_guard.failed(now);
    return error("Wrong PIN", 403);
  }
  s_guard.succeeded();
  plat::WebResponse res = ok();
  res.set_cookie = "radar=" + s_token + "; Path=/; Max-Age=31536000; SameSite=Strict; HttpOnly";
  return res;
}

std::string boolJson(bool v) { return v ? "true" : "false"; }

plat::WebResponse status() {
  std::string body;
  const plat::WifiStatus ws = plat::wifiStatus();
  const FeedStatus fs = feedStatus();
  body = "{\"version\":" + jsonString(plat::firmwareVersion()) + ",\"board\":" + jsonString(plat::firmwareVariant()) +
         ",\"aircraft\":" + std::to_string(fs.last_count) + ",\"ssid\":" + jsonString(ws.ssid) +
         ",\"ip\":" + jsonString(ws.ip) + ",\"rssi\":" + std::to_string(ws.rssi) +
         ",\"uptime_s\":" + std::to_string(plat::millis() / 1000) + ",\"units\":\"" + unitSuffix() + "\"}";
  return json(body);
}

plat::WebResponse getSettings() {
  std::string body;
  if (!runOnUi([&] { body = core::settings_text::toJson(settings()); })) return busy();
  return json(body);
}

plat::WebResponse postSettings(const plat::WebRequest& r) {
  const std::string text = core::web::parseForm(r.body)["text"];
  std::vector<std::string> problems;
  int n = 0;
  if (!runOnUi([&] { n = applySettingsText(text, &problems); })) return busy();
  std::string list;
  for (const std::string& p : problems) list += (list.empty() ? "" : ",") + jsonString(p);
  std::printf("web: %s -> %d read, %u problems\n", text.c_str(), n, static_cast<unsigned>(problems.size()));
  return ok(",\"read\":" + std::to_string(n) + ",\"problems\":[" + list + "]");
}

plat::WebResponse getFavorites() {
  const std::vector<core::Favorite> favs = favorites();
  std::vector<core::Track> tracks;
  traffic().snapshot(tracks);
  std::string list;
  for (const core::Favorite& f : favs) {
    bool in_range = false;
    for (const core::Track& t : tracks) in_range |= std::strcmp(t.ac.hex, f.hex) == 0;
    list += std::string(list.empty() ? "" : ",") + "{\"hex\":" + jsonString(f.hex) + ",\"label\":" + jsonString(f.label) +
            ",\"type\":" + jsonString(f.type) + ",\"in_range\":" + boolJson(in_range) + "}";
  }
  std::string unresolved;
  for (const std::string& u : unresolvedFavorites()) unresolved += (unresolved.empty() ? "" : ",") + jsonString(u);
  return json("{\"max\":" + std::to_string(core::kMaxFavorites) + ",\"list\":[" + list + "],\"unresolved\":[" +
              unresolved + "]}");
}

/** Tail numbers and hex codes: letters, digits and dashes. */
bool plausibleTail(const std::string& q) {
  if (q.empty() || q.size() > 11) return false;
  for (char c : q) {
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-') return false;
  }
  return true;
}

plat::WebResponse favoriteAction(const std::string& what, const plat::WebRequest& r) {
  auto f = core::web::parseForm(r.body);
  if (what == "add") {
    std::string q;
    for (char c : f["q"]) {
      if (c != ' ') q.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    if (!plausibleTail(q)) return error("That doesn't look like a tail number or hex code");
    if (favorites().size() >= core::kMaxFavorites) return error("Favorites full (32) - remove one first");
    lookupFavorite(q);
    return ok();
  }
  const std::string hex = f["hex"];
  if (what == "remove") {
    removeFavorite(hex.c_str());
    return ok();
  }
  if (what == "rename") {
    const std::string label = f["label"];
    if (label.empty() || label.size() > 11) return error("Labels are 1 to 11 characters");
    renameFavorite(hex.c_str(), label.c_str());
    return ok();
  }
  return error("Unknown request", 404);
}

const char* wifiStateName(plat::WifiState s) {
  switch (s) {
    case plat::WifiState::Connected: return "connected";
    case plat::WifiState::Connecting: return "connecting";
    case plat::WifiState::Failed: return "not connected";
    default: return "off";
  }
}

plat::WebResponse getWifi() {
  std::vector<core::SavedWifi> saved;
  if (!runOnUi([&] { saved = savedWifi(); })) return busy();
  const plat::WifiStatus ws = plat::wifiStatus();
  std::string list;
  for (const core::SavedWifi& w : saved) list += (list.empty() ? "" : ",") + jsonString(w.ssid);
  return json("{\"current\":{\"state\":\"" + std::string(wifiStateName(ws.state)) + "\",\"ssid\":" + jsonString(ws.ssid) +
              ",\"ip\":" + jsonString(ws.ip) + ",\"rssi\":" + std::to_string(ws.rssi) + "},\"saved\":[" + list +
              "],\"max\":" + std::to_string(1 + core::kMaxSavedWifi) + "}");
}

plat::WebResponse wifiScan(bool start) {
  if (start) {
    if (!runOnUi([] {
          s_scan_done = false;
          plat::wifiScanStart();
        })) {
      return busy();
    }
    return ok();
  }
  bool done = false;
  std::vector<plat::WifiNetwork> nets;
  if (!runOnUi([&] {
        std::vector<plat::WifiNetwork> fresh;
        if (plat::wifiScanResults(&fresh)) {
          s_scan = fresh;
          s_scan_done = true;
        }
        done = s_scan_done;
        nets = s_scan;
      })) {
    return busy();
  }
  std::string list;
  for (const plat::WifiNetwork& n : nets) {
    list += std::string(list.empty() ? "" : ",") + "{\"ssid\":" + jsonString(n.ssid) + ",\"rssi\":" +
            std::to_string(n.rssi) + ",\"secure\":" + boolJson(n.secure) + "}";
  }
  return json("{\"done\":" + boolJson(done) + ",\"networks\":[" + list + "]}");
}

plat::WebResponse wifiAction(const std::string& what, const plat::WebRequest& r) {
  auto f = core::web::parseForm(r.body);
  const std::string ssid = f["ssid"], pass = f["pass"];
  if (ssid.empty() || ssid.size() > 32) return error("Network names are 1 to 32 characters");
  if (pass.size() > 64) return error("Passwords are up to 64 characters");
  bool result = true;
  bool ran = runOnUi([&] {
    if (what == "add") {
      if (f["now"] == "1") useWifi(ssid.c_str(), pass.c_str());
      else result = addWifi(ssid.c_str(), pass.c_str());
    } else if (what == "use") {
      std::string saved_pass;
      result = isSavedWifi(ssid.c_str(), &saved_pass);
      if (result) useWifi(ssid.c_str(), saved_pass.c_str());
    } else if (what == "forget") {
      forgetWifi(ssid.c_str());
    } else if (what == "up") {
      moveWifiUp(ssid.c_str());
    } else {
      result = false;
    }
  });
  if (!ran) return busy();
  if (!result) return error(what == "add" ? "All 5 network slots are taken - forget one first" : "Not a saved network");
  return ok();
}

plat::WebResponse getDayLog() {
  std::string today, days;
  bool sd = false;
  if (!runOnUi([&] {
        const core::daylog::Day d = dayLogToday();
        const bool h24 = settings().clock_24h;
        auto pair = [&today](const char* k, const std::string& v) {
          today += std::string(today.empty() ? "" : ",") + "[" + jsonString(k) + "," + jsonString(v) + "]";
        };
        auto hm = [h24](int minute) {
          char b[16];
          core::formatTime(minute / 60, minute % 60, h24, b, sizeof(b));
          return std::string(b);
        };
        auto rec = [&](const core::daylog::Record& r, const std::string& what) {
          if (r.minute < 0) return std::string("-");
          return std::string(r.label) + (r.type[0] ? " " : "") + r.type + ", " + what + ", " + hm(r.minute);
        };
        if (d.date == 0) {
          pair("Today", "Waiting for the time (needs internet)");
        } else {
          char date[16];
          std::snprintf(date, sizeof(date), "%04d-%02d-%02d", d.date / 10000, d.date / 100 % 100, d.date % 100);
          pair("Date", date);
          pair("Aircraft", std::to_string(d.count()) + " different");
          pair("Busiest", d.peak_minute >= 0 ? std::to_string(d.peak) + " at once, " + hm(d.peak_minute) : "-");
          char dist[24];
          formatDistance(d.closest.value, dist, sizeof(dist));
          pair("Closest pass", rec(d.closest, std::string(dist) + " at " + std::to_string(d.closest.alt_ft) + " ft"));
          pair("Highest", rec(d.highest, std::to_string(static_cast<int>(d.highest.value)) + " ft"));
          pair("Fastest", rec(d.fastest, std::to_string(static_cast<int>(d.fastest.value)) + " kt"));
          pair("Military / emergencies",
               std::to_string(d.military.size()) + " / " + std::to_string(d.emergencies.size()));
          std::string favs;
          for (const std::string& x : d.favorites) favs += (favs.empty() ? "" : ", ") + x;
          pair("Favorites seen", favs.empty() ? "-" : favs);
        }
        for (const core::daylog::Day& day : dayLogHistory(7)) {
          char dist[24];
          formatDistance(day.closest.value, dist, sizeof(dist));
          days += (days.empty() ? "" : ",") + jsonString(core::daylog::summary(day, dist));
        }
        sd = maps::sdStatus().state == maps::SdState::Mounted;
      })) {
    return busy();
  }
  return json("{\"sd\":" + boolJson(sd) + ",\"today\":[" + today + "],\"days\":[" + days + "]}");
}

plat::WebResponse getShots() {
  std::string list;
  for (const std::string& n : screenshots()) list += (list.empty() ? "" : ",") + jsonString(n);
  return json("{\"sd\":" + boolJson(maps::sdStatus().state == maps::SdState::Mounted) + ",\"files\":[" + list + "]}");
}

plat::WebResponse getShot(const std::string& name) {
  const std::string path = screenshotPath(core::web::urlDecode(name));
  FILE* f = path.empty() ? nullptr : std::fopen(path.c_str(), "rb");
  if (f == nullptr) return error("No such screenshot", 404);
  plat::WebResponse r;
  r.type = "image/png";
  char buf[4096];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) r.body.append(buf, n);
  std::fclose(f);
  return r;
}

plat::WebResponse takeShot() {
  bool taken = false;
  if (!runOnUi([&] { taken = requestScreenshot(); })) return busy();
  if (!taken) return error("Couldn't take one (no SD card, or still saving the last)");
  return ok();
}

/** A firmware file, streamed into the idle app slot. */
plat::WebResponse firmware(plat::WebRequest& r) {
  if (!r.read || r.content_length < core::FirmwareCheck::kHeaderBytes) return error("That file is too small to be firmware");
  if (!uploadBegin(r.content_length)) return error("An update is already being installed", 409);
  std::string err;
  if (!plat::firmwareWriteBegin(&err)) {
    uploadEnd(false, err);
    return error(err, 500);
  }
  core::FirmwareCheck check("plane_radar_p4", plat::firmwareVariant());
  std::vector<char> buf(8192);
  std::string head;  // held back until the header is checked
  size_t done = 0;
  bool header_ok = false;
  auto fail = [&](const std::string& why) {
    plat::firmwareWriteEnd(false, &err);
    uploadEnd(false, why);
    std::printf("web: firmware upload refused: %s\n", why.c_str());
    return error(why);
  };
  while (done < r.content_length) {
    const int n = r.read(buf.data(), std::min(buf.size(), r.content_length - done));
    if (n <= 0) return fail("The upload stopped - try again");
    done += static_cast<size_t>(n);
    check.feed(reinterpret_cast<const uint8_t*>(buf.data()), static_cast<size_t>(n));
    if (!header_ok) {
      head.append(buf.data(), static_cast<size_t>(n));
      if (head.size() < core::FirmwareCheck::kHeaderBytes) continue;
      const std::string p = check.headerProblem();
      if (!p.empty()) return fail(p);
      header_ok = true;
      if (!plat::firmwareWrite(head.data(), head.size(), &err)) return fail(err);
      head.clear();
    } else if (!plat::firmwareWrite(buf.data(), static_cast<size_t>(n), &err)) {
      return fail(err);
    }
    uploadProgress(done, check.version().c_str());
  }
  if (done != r.content_length) return fail("The upload stopped - try again");
  const std::string p = check.finalProblem();
  if (!p.empty()) return fail(p);
  if (!plat::firmwareWriteEnd(true, &err)) {
    uploadEnd(false, err);
    return error(err);
  }
  uploadEnd(true, "");
  std::printf("web: installed %s from an uploaded file\n", check.version().c_str());
  return ok(",\"version\":" + jsonString(check.version()));
}

plat::WebResponse handle(plat::WebRequest& r) {
  const std::string& p = r.path;
  if (r.method == "GET" && (p == "/" || p == "/index.html")) {
    plat::WebResponse res;
    res.type = "text/html; charset=utf-8";
    res.body = core::web::kPage;
    return res;
  }
  if (p == "/api/login" && r.method == "POST") return login(r);
  if (p.compare(0, 5, "/api/") != 0 && p.compare(0, 7, "/shots/") != 0) return error("Not found", 404);
  if (!loggedIn(r)) return error("login", 401);

  const bool post = r.method == "POST";
  if (p == "/api/status") return status();
  if (p == "/api/settings") return post ? postSettings(r) : getSettings();
  if (p == "/api/favorites") return getFavorites();
  if (post && p.compare(0, 15, "/api/favorites/") == 0) return favoriteAction(p.substr(15), r);
  if (p == "/api/wifi") return getWifi();
  if (p == "/api/wifi/scan") return wifiScan(post);
  if (post && p.compare(0, 10, "/api/wifi/") == 0) return wifiAction(p.substr(10), r);
  if (p == "/api/daylog") return getDayLog();
  if (p == "/api/shots") return getShots();
  if (p.compare(0, 7, "/shots/") == 0) return getShot(p.substr(7));
  if (post && p == "/api/screenshot") return takeShot();
  if (post && p == "/api/firmware") return firmware(r);
  if (post && p == "/api/restart") {
    runOnUi([] { s_restart_at_ms = plat::millis() + 1500; });
    return ok();
  }
  return error("Not found", 404);
}

}  // namespace

void service() {
  core::Settings& st = settings();
  if (st.web_pin[0] == '\0' || st.web_token[0] == '\0') {
    std::snprintf(st.web_pin, sizeof(st.web_pin), "%s", randomDigits(4).c_str());
    std::snprintf(st.web_token, sizeof(st.web_token), "%s", randomHex(16).c_str());
    settingsChanged();
  }
  {
    std::lock_guard<std::mutex> lock(s_mu);
    s_pin = st.web_pin;
    s_token = st.web_token;
  }
  if (st.web_enabled && !s_running) {
    s_running = plat::webServerStart(handle);
    std::printf("web: %s\n", s_running ? "phone page running" : "phone page FAILED to start");
    if (!s_running) st.web_enabled = false;  // don't retry every tick
  } else if (!st.web_enabled && s_running) {
    plat::webServerStop();
    s_running = false;
    std::printf("web: phone page stopped\n");
  }
  if (s_restart_at_ms != 0 && static_cast<int32_t>(plat::millis() - s_restart_at_ms) >= 0) {
    settingsFlush();
    plat::restart();
  }
}

bool running() { return s_running; }

std::string address() {
  const plat::WifiStatus ws = plat::wifiStatus();
  if (ws.state != plat::WifiState::Connected || ws.ip[0] == '\0') return "";
  std::string a = std::string("http://") + ws.ip;
#ifdef SIM_WEB_PORT
  a += ":" + std::to_string(SIM_WEB_PORT);
#endif
  return a;
}

std::string loginUrl() {
  const std::string a = address();
  return a.empty() ? "" : a + "/?pin=" + settings().web_pin;
}

void newPin() {
  core::Settings& st = settings();
  st.web_pin[0] = '\0';
  st.web_token[0] = '\0';
  service();
  settingsFlush();
}

}  // namespace app::web
