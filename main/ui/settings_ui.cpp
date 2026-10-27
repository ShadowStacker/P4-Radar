#include "ui/settings_ui.h"

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "app/app.h"
#include "core/adsb.h"
#include "core/filter.h"
#include "data/airports.h"
#include "platform/platform.h"
#include "ui/hud.h"
#include "ui/keyboard.h"
#include "ui/radar_view.h"
#include "ui/sprites.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace ui::settings_ui {

namespace {

using Fmt = std::function<std::string(int)>;

struct State {
  lv_obj_t* screen = nullptr;
  lv_obj_t* title = nullptr;
  lv_obj_t* list = nullptr;
  lv_timer_t* timer = nullptr;
  Page page = Page::Main;
  std::function<void()> tick;
  // Wi-Fi page
  lv_obj_t* wifi_status = nullptr;
  lv_obj_t* wifi_list = nullptr;
  bool scanning = false;
};

State s;

void showPage(Page page);

void changed() {
  app::settingsChanged();
  RadarView::get().markStaticDirty();
}

std::string fmtStr(const char* f, ...) __attribute__((format(printf, 1, 2)));
std::string fmtStr(const char* f, ...) {
  char buf[96];
  va_list ap;
  va_start(ap, f);
  std::vsnprintf(buf, sizeof(buf), f, ap);
  va_end(ap);
  return buf;
}

// ---------------------------------------------------------------------------
// Row builders

lv_obj_t* baseRow(lv_obj_t* parent) {
  const Palette& p = pal();
  lv_obj_t* r = lv_obj_create(parent);
  lv_obj_set_width(r, lv_pct(100));
  lv_obj_set_height(r, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(r, p.panel, 0);
  lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(r, p.panel_border, 0);
  lv_obj_set_style_border_width(r, sc(2), 0);
  lv_obj_set_style_radius(r, sc(22), 0);
  lv_obj_set_style_pad_hor(r, sc(22), 0);
  lv_obj_set_style_pad_ver(r, sc(18), 0);
  lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(r, sc(14), 0);
  return r;
}

lv_obj_t* growLabel(lv_obj_t* row, const char* text, lv_color_t c, FontSize fs = FontSize::M) {
  lv_obj_t* l = label(row, text, font(fs), c);
  lv_obj_set_flex_grow(l, 1);
  lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
  lv_obj_set_height(l, lv_font_get_line_height(font(fs)));  // one line, ellipsized
  return l;
}

void section(const char* text) {
  lv_obj_t* l = label(s.list, text, font(FontSize::S), pal().accent);
  lv_obj_set_style_pad_top(l, sc(10), 0);
  lv_obj_set_style_pad_left(l, sc(16), 0);
}

lv_obj_t* navRow(const char* symbol, const char* text, const std::string& value, Page target) {
  const Palette& p = pal();
  lv_obj_t* r = baseRow(s.list);
  lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(r, p.panel_border, LV_STATE_PRESSED);
  label(r, symbol, font(FontSize::L), p.accent);
  growLabel(r, text, p.text);
  lv_obj_t* v = label(r, value.c_str(), font(FontSize::S), p.text_dim);
  lv_obj_set_style_max_width(v, sc(220), 0);
  label(r, LV_SYMBOL_RIGHT, font(FontSize::S), p.text_dim);
  onClick(r, [target] { showPage(target); });
  return r;
}

lv_obj_t* actionRow(const char* symbol, const char* text, Callback cb, lv_color_t color) {
  const Palette& p = pal();
  lv_obj_t* r = baseRow(s.list);
  lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(r, p.panel_border, LV_STATE_PRESSED);
  label(r, symbol, font(FontSize::L), color);
  growLabel(r, text, color);
  onClick(r, std::move(cb));
  return r;
}

lv_obj_t* infoRow(const char* title, const std::string& value) {
  const Palette& p = pal();
  lv_obj_t* r = baseRow(s.list);
  growLabel(r, title, p.text_dim, FontSize::S);
  lv_obj_t* v = label(r, value.c_str(), font(FontSize::M), p.text);
  lv_obj_set_style_max_width(v, sc(360), 0);
  lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_RIGHT, 0);
  return v;
}

void switchRow(const char* text, bool* value, Callback extra = nullptr) {
  const Palette& p = pal();
  lv_obj_t* r = baseRow(s.list);
  growLabel(r, text, p.text);
  lv_obj_t* sw = lv_switch_create(r);
  lv_obj_set_size(sw, sc(92), sc(48));
  lv_obj_set_style_bg_color(sw, p.accent, static_cast<lv_style_selector_t>(LV_PART_INDICATOR) | LV_STATE_CHECKED);
  if (*value) lv_obj_add_state(sw, LV_STATE_CHECKED);
  onEvent(sw, LV_EVENT_VALUE_CHANGED, [sw, value, extra] {
    *value = lv_obj_has_state(sw, LV_STATE_CHECKED);
    app::sound(core::Sound::Click);
    changed();
    if (extra) extra();
  });
}

struct SliderCtx {
  lv_obj_t* value_label;
  std::function<void(int)> set;
  Fmt fmt;
  Callback on_release;
};

void onSliderEvent(lv_event_t* e) {
  auto* ctx = static_cast<SliderCtx*>(lv_event_get_user_data(e));
  lv_obj_t* sl = static_cast<lv_obj_t*>(lv_event_get_target(e));
  switch (lv_event_get_code(e)) {
    case LV_EVENT_VALUE_CHANGED: {
      const int v = lv_slider_get_value(sl);
      lv_label_set_text(ctx->value_label, ctx->fmt(v).c_str());
      ctx->set(v);
      changed();
      break;
    }
    case LV_EVENT_RELEASED:
      if (ctx->on_release) ctx->on_release();
      break;
    case LV_EVENT_DELETE: delete ctx; break;
    default: break;
  }
}

void sliderRow(const char* text, int min, int max, int value, std::function<void(int)> set, Fmt fmt,
               Callback on_release = nullptr) {
  const Palette& p = pal();
  lv_obj_t* r = baseRow(s.list);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_row(r, sc(14), 0);
  growLabel(r, text, p.text);
  lv_obj_t* v = label(r, fmt(value).c_str(), font(FontSize::M), p.accent);

  lv_obj_t* sl = lv_slider_create(r);
  lv_obj_set_width(sl, lv_pct(96));
  lv_obj_set_height(sl, sc(14));
  lv_obj_set_style_pad_bottom(r, sc(22), 0);
  lv_obj_set_ext_click_area(sl, sc(24));
  lv_slider_set_range(sl, min, max);
  lv_slider_set_value(sl, std::clamp(value, min, max), LV_ANIM_OFF);
  lv_obj_set_style_bg_color(sl, p.accent, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(sl, p.accent, LV_PART_KNOB);
  lv_obj_set_style_pad_all(sl, sc(8), LV_PART_KNOB);
  lv_obj_set_style_bg_color(sl, p.panel_border, LV_PART_MAIN);

  auto* ctx = new SliderCtx{v, std::move(set), std::move(fmt), std::move(on_release)};
  lv_obj_add_event_cb(sl, onSliderEvent, LV_EVENT_VALUE_CHANGED, ctx);
  lv_obj_add_event_cb(sl, onSliderEvent, LV_EVENT_RELEASED, ctx);
  lv_obj_add_event_cb(sl, onSliderEvent, LV_EVENT_DELETE, ctx);
}

/** Segmented control. `map` must be a static, "" terminated button map. */
void choiceRow(const char* text, const char* const* map, int current, std::function<void(int)> set) {
  const Palette& p = pal();
  lv_obj_t* r = baseRow(s.list);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(r, sc(10), 0);
  label(r, text, font(FontSize::M), p.text);

  lv_obj_t* bm = lv_buttonmatrix_create(r);
  lv_buttonmatrix_set_map(bm, map);
  lv_obj_set_size(bm, lv_pct(100), sc(64));
  lv_buttonmatrix_set_button_ctrl_all(bm, LV_BUTTONMATRIX_CTRL_CHECKABLE);
  lv_buttonmatrix_set_one_checked(bm, true);
  lv_buttonmatrix_set_button_ctrl(bm, static_cast<uint32_t>(current), LV_BUTTONMATRIX_CTRL_CHECKED);
  lv_obj_set_style_bg_opa(bm, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(bm, 0, 0);
  lv_obj_set_style_pad_all(bm, 0, 0);
  lv_obj_set_style_pad_gap(bm, sc(6), 0);
  lv_obj_set_style_text_font(bm, font(FontSize::S), LV_PART_ITEMS);
  lv_obj_set_style_bg_color(bm, p.bg, LV_PART_ITEMS);
  lv_obj_set_style_text_color(bm, p.text, LV_PART_ITEMS);
  lv_obj_set_style_radius(bm, sc(14), LV_PART_ITEMS);
  lv_obj_set_style_bg_color(bm, p.accent, static_cast<lv_style_selector_t>(LV_PART_ITEMS) | LV_STATE_CHECKED);
  lv_obj_set_style_text_color(bm, lv_color_black(), static_cast<lv_style_selector_t>(LV_PART_ITEMS) | LV_STATE_CHECKED);
  onEvent(bm, LV_EVENT_VALUE_CHANGED, [bm, set] {
    const uint32_t sel = lv_buttonmatrix_get_selected_button(bm);
    if (sel == LV_BUTTONMATRIX_BUTTON_NONE) return;
    app::sound(core::Sound::Click);
    set(static_cast<int>(sel));
    changed();
  });
}

// ---------------------------------------------------------------------------
// Pages

void pageMain() {
  const auto& st = app::settings();
  const plat::WifiStatus ws = plat::wifiStatus();
  std::string wifi = st.wifi_ssid[0] ? st.wifi_ssid : "Not set";
  if (ws.state == plat::WifiState::Connected) wifi = std::string(LV_SYMBOL_OK " ") + ws.ssid;

  navRow(LV_SYMBOL_WIFI, "Wi-Fi", wifi, Page::Wifi);
  navRow(LV_SYMBOL_HOME, "Location", fmtStr("%.3f, %.3f", st.home_lat, st.home_lon), Page::Location);
  navRow(LV_SYMBOL_IMAGE, "Display", "", Page::Display);
  navRow(LV_SYMBOL_VOLUME_MAX, "Sound", st.sound_enabled ? fmtStr("%d%%", st.volume) : "Off", Page::Sound);
  navRow(LV_SYMBOL_GPS, "Traffic & data", core::adsb::sourceName(st.source), Page::Traffic);
  navRow(LV_SYMBOL_EYE_OPEN, "Filters", core::filtersActive(st) ? "On" : "All aircraft", Page::Filters);
  navRow(LV_SYMBOL_BELL, "Favorites", fmtStr("%u", static_cast<unsigned>(app::favorites().size())), Page::Favorites);
  navRow(LV_SYMBOL_LIST, "About", plat::firmwareVersion(), Page::About);
}

void connectTo(const std::string& ssid, const std::string& pass) {
  auto& st = app::settings();
  std::snprintf(st.wifi_ssid, sizeof(st.wifi_ssid), "%s", ssid.c_str());
  std::snprintf(st.wifi_pass, sizeof(st.wifi_pass), "%s", pass.c_str());
  app::settingsChanged();
  app::settingsFlush();
  plat::wifiConnect(st.wifi_ssid, st.wifi_pass);
  if (!st.home_set) {
    // First-time setup continues with the location.
    toast((std::string(LV_SYMBOL_WIFI "  Connecting... now set your home location")).c_str(), 3500);
    showPage(Page::Location);
  } else {
    toast((std::string(LV_SYMBOL_WIFI "  Connecting to ") + ssid).c_str());
  }
}

void askPassword(const std::string& ssid) {
  const std::string title = "Password for " + ssid;
  const auto& st = app::settings();
  const std::string initial = ssid == st.wifi_ssid ? st.wifi_pass : "";
  textInput(title.c_str(), initial, InputKind::Password,
            [ssid](const std::string& pass) { connectTo(ssid, pass); });
}

void fillWifiList(const std::vector<plat::WifiNetwork>& nets) {
  const Palette& p = pal();
  lv_obj_clean(s.wifi_list);
  if (nets.empty()) {
    label(s.wifi_list, "No networks found", font(FontSize::M), p.text_dim);
    return;
  }
  for (const auto& n : nets) {
    lv_obj_t* r = baseRow(s.wifi_list);
    lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(r, p.panel_border, LV_STATE_PRESSED);
    const int bars = n.rssi > -55 ? 4 : n.rssi > -65 ? 3 : n.rssi > -75 ? 2 : 1;
    const lv_color_t c = bars >= 3 ? p.accent : (bars == 2 ? p.warn : p.danger);
    label(r, LV_SYMBOL_WIFI, font(FontSize::M), c);
    growLabel(r, n.ssid.c_str(), p.text);
    if (n.secure) label(r, LV_SYMBOL_EYE_CLOSE, font(FontSize::S), p.text_dim);
    const std::string ssid = n.ssid;
    const bool secure = n.secure;
    onClick(r, [ssid, secure] {
      if (secure) askPassword(ssid);
      else connectTo(ssid, "");
    });
  }
}

void startScan() {
  s.scanning = true;
  plat::wifiScanStart();
  lv_obj_clean(s.wifi_list);
  label(s.wifi_list, LV_SYMBOL_REFRESH "  Scanning...", font(FontSize::M), pal().text_dim);
}

void pageWifi() {
  s.wifi_status = infoRow("Status", "");
  section("NETWORKS");
  s.wifi_list = lv_obj_create(s.list);
  lv_obj_remove_style_all(s.wifi_list);
  lv_obj_set_size(s.wifi_list, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(s.wifi_list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(s.wifi_list, sc(10), 0);

  actionRow(LV_SYMBOL_REFRESH, "Scan again", [] { startScan(); }, pal().text);
  actionRow(LV_SYMBOL_KEYBOARD, "Hidden network...", [] {
    textInput("Network name (SSID)", "", InputKind::Text, [](const std::string& ssid) {
      if (!ssid.empty()) askPassword(ssid);
    });
  }, pal().text);
  actionRow(LV_SYMBOL_TRASH, "Forget saved network", [] {
    confirm("Forget Wi-Fi?", "The radar will disconnect until you choose a network again.", "Forget", [] {
      auto& st = app::settings();
      st.wifi_ssid[0] = '\0';
      st.wifi_pass[0] = '\0';
      app::settingsChanged();
      app::settingsFlush();
      plat::wifiConnect("", "");
    });
  }, pal().danger);

  startScan();
  s.tick = [] {
    const plat::WifiStatus ws = plat::wifiStatus();
    std::string txt;
    switch (ws.state) {
      case plat::WifiState::Connected: txt = fmtStr("%s  (%s, %d dBm)", ws.ssid, ws.ip, ws.rssi); break;
      case plat::WifiState::Connecting: txt = fmtStr("Connecting to %s...", ws.ssid); break;
      case plat::WifiState::Failed: txt = fmtStr("Failed: %s", ws.reason); break;
      default: txt = app::settings().wifi_ssid[0] ? "Disconnected" : "Not configured"; break;
    }
    lv_label_set_text(s.wifi_status, txt.c_str());
    if (s.scanning) {
      std::vector<plat::WifiNetwork> nets;
      if (plat::wifiScanResults(&nets)) {
        s.scanning = false;
        fillWifiList(nets);
      }
    }
  };
}

void setHome(double lat, double lon, const char* what) {
  auto& st = app::settings();
  st.home_lat = lat;
  st.home_lon = lon;
  st.home_set = true;
  changed();
  app::settingsFlush();
  RadarView::get().goHome(false);
  toast((std::string(LV_SYMBOL_OK "  Home set to ") + what).c_str(), 2500);
  showPage(Page::Location);
}

const data::Airport* findAirport(const std::string& query) {
  std::string q;
  for (char c : query) {
    if (c != ' ') q += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  }
  if (q.empty()) return nullptr;
  // Exact ICAO/IATA/FAA code first; then US codes typed without the "K" (BDN -> KBDN).
  const std::string kq = "K" + q;
  const data::Airport* k_match = nullptr;
  for (size_t i = 0; i < data::kAirportCount; ++i) {
    const data::Airport& a = data::kAirports[i];
    if (q == a.ident || q == a.iata) return &a;
    if (k_match == nullptr && kq == a.ident) k_match = &a;
  }
  return k_match;
}

void pageLocation() {
  auto& st = app::settings();
  infoRow("Latitude", fmtStr("%.5f", st.home_lat));
  infoRow("Longitude", fmtStr("%.5f", st.home_lon));

  actionRow(LV_SYMBOL_GPS, "Pick on the map", [] {
    closeSettings();
    hud::enterPickMode();
  }, pal().accent);
  actionRow(LV_SYMBOL_KEYBOARD, "Enter coordinates", [] {
    const auto& st = app::settings();
    textInput("Latitude (-90 to 90)", fmtStr("%.5f", st.home_lat), InputKind::Number, [](const std::string& lat_s) {
      const double lat = std::atof(lat_s.c_str());
      if (lat < -90 || lat > 90) {
        toast(LV_SYMBOL_WARNING "  Latitude must be -90..90");
        return;
      }
      textInput("Longitude (-180 to 180)", fmtStr("%.5f", app::settings().home_lon), InputKind::Number,
                [lat](const std::string& lon_s) {
                  const double lon = std::atof(lon_s.c_str());
                  if (lon < -180 || lon > 180) {
                    toast(LV_SYMBOL_WARNING "  Longitude must be -180..180");
                    return;
                  }
                  setHome(lat, lon, fmtStr("%.4f, %.4f", lat, lon).c_str());
                });
    });
  }, pal().text);
  actionRow(LV_SYMBOL_UPLOAD, "Use an airport (ICAO/IATA/FAA)", [] {
    textInput("Airport code, e.g. KRDM, BDN or S39", "", InputKind::Text, [](const std::string& q) {
      const data::Airport* a = findAirport(q);
      if (a == nullptr) {
        toast(LV_SYMBOL_WARNING "  Airport not found");
        return;
      }
      setHome(a->lat_e7 * 1e-7, a->lon_e7 * 1e-7, a->name);
    });
  }, pal().text);

  section("HOME VIEW");
  sliderRow("Home zoom (radius)", 2, 250, static_cast<int>(app::kmToUnits(st.home_radius_km) + 0.5),
            [](int v) { app::settings().home_radius_km = static_cast<float>(app::unitsToKm(v)); },
            [](int v) { return fmtStr("%d %s", v, app::unitSuffix()); });
}

void pageDisplay() {
  auto& st = app::settings();
  static const char* const kThemes[] = {"Green", "Amber", "Blue", "Night", ""};
  choiceRow("Theme", kThemes, static_cast<int>(st.theme), [](int v) {
    app::settings().theme = static_cast<core::Theme>(v);
    app::settingsChanged();
    lv_async_call([](void*) { rebuild(); }, nullptr);  // rebuild every screen with new colors
  });
  sliderRow("Brightness", 5, 100, st.brightness, [](int v) { app::settings().brightness = static_cast<uint8_t>(v); },
            [](int v) { return fmtStr("%d%%", v); });
  static const char* const kUnits[] = {"km", "mi", "nm", ""};
  choiceRow("Distance units", kUnits, static_cast<int>(st.units),
            [](int v) { app::settings().units = static_cast<core::Units>(v); });

  switchRow("Auto-hide buttons after 10 s", &st.autohide_buttons);

  section("AIRCRAFT");
  static const char* const kLabels[] = {"Off", "Callsign", "Full", ""};
  choiceRow("Labels", kLabels, static_cast<int>(st.labels),
            [](int v) { app::settings().labels = static_cast<core::LabelMode>(v); });
  static const char* const kColors[] = {"Theme", "By altitude", ""};
  choiceRow("Aircraft colors", kColors, static_cast<int>(st.color_mode),
            [](int v) { app::settings().color_mode = static_cast<core::ColorMode>(v); });
  switchRow("Trails", &st.show_trails);
  switchRow("Speed vectors (1 min)", &st.show_speed_vectors);

  section("MAP");
  switchRow("Range rings", &st.show_rings);
  switchRow("Runways", &st.show_runways);
  switchRow("Airport codes", &st.show_airport_labels);

  section("ANIMATION");
  switchRow("Radar sweep", &st.sweep_enabled);
  sliderRow("Sweep speed", 4, 60, st.sweep_rpm, [](int v) { app::settings().sweep_rpm = static_cast<uint8_t>(v); },
            [](int v) { return fmtStr("%d rpm", v); });
  sliderRow("Sweep trail", 0, 36, st.sweep_trail_deg / 5,
            [](int v) { app::settings().sweep_trail_deg = static_cast<uint8_t>(v * 5); },
            [](int v) { return v == 0 ? std::string("Clean bar") : fmtStr("%d" "\xC2\xB0", v * 5); });
  switchRow("Smooth trail fade", &st.sweep_smooth);
  switchRow("Echoes fade after the beam", &st.sweep_reveal);

  section("POWER");
  static const char* const kDim[] = {"Never", "1 min", "5 min", "15 min", "30 min", ""};
  static const uint8_t kDimVals[] = {0, 1, 5, 15, 30};
  int cur = 0;
  for (int i = 0; i < 5; ++i) {
    if (kDimVals[i] == st.dim_after_min) cur = i;
  }
  choiceRow("Dim when idle", kDim, cur, [](int v) { app::settings().dim_after_min = kDimVals[v]; });
  sliderRow("Dimmed brightness", 0, 50, st.dim_brightness,
            [](int v) { app::settings().dim_brightness = static_cast<uint8_t>(v); },
            [](int v) { return fmtStr("%d%%", v); });
}

void pageSound() {
  auto& st = app::settings();
  switchRow("Sound", &st.sound_enabled);
  sliderRow("Volume", 0, 100, st.volume, [](int v) { app::settings().volume = static_cast<uint8_t>(v); },
            [](int v) { return fmtStr("%d%%", v); }, [] { plat::playSound(core::Sound::NewAircraft); });

  section("PLAY A SOUND FOR");
  switchRow("Touches", &st.sound_touch);
  switchRow("New aircraft", &st.sound_new_aircraft);
  switchRow("Each sweep (sonar ping)", &st.sound_sweep_ping);
  switchRow("Emergency squawks", &st.sound_alerts);

  section("PROXIMITY ALERT (ANY AIRCRAFT)");
  switchRow("Proximity alert", &st.sound_proximity);
  sliderRow("Alert radius", 1, 20, static_cast<int>(app::kmToUnits(st.alert_radius_km) + 0.5),
            [](int v) { app::settings().alert_radius_km = static_cast<float>(app::unitsToKm(v)); },
            [](int v) { return fmtStr("%d %s", v, app::unitSuffix()); });
  sliderRow("Below altitude", 1, 40, st.alert_max_alt_ft / 500,
            [](int v) { app::settings().alert_max_alt_ft = v * 500; },
            [](int v) { return fmtStr("%d ft", v * 500); });

  section("FAVORITES");
  switchRow("Favorite detected", &st.sound_favorite_seen);
  switchRow("Favorite nearby", &st.sound_favorite_near);
  sliderRow("Nearby radius", 1, 100, static_cast<int>(app::kmToUnits(st.favorite_near_radius_km) + 0.5),
            [](int v) { app::settings().favorite_near_radius_km = static_cast<float>(app::unitsToKm(v)); },
            [](int v) { return fmtStr("%d %s", v, app::unitSuffix()); });

  section("TEST");
  actionRow(LV_SYMBOL_PLAY, "Sonar ping", [] { plat::playSound(core::Sound::Ping); }, pal().text);
  actionRow(LV_SYMBOL_PLAY, "New aircraft chime", [] { plat::playSound(core::Sound::NewAircraft); }, pal().text);
  actionRow(LV_SYMBOL_PLAY, "Proximity alert", [] { plat::playSound(core::Sound::Alert); }, pal().text);
  actionRow(LV_SYMBOL_PLAY, "Favorite detected", [] { plat::playSound(core::Sound::FavoriteSeen); }, pal().text);
  actionRow(LV_SYMBOL_PLAY, "Favorite nearby", [] { plat::playSound(core::Sound::FavoriteNear); }, pal().text);
  actionRow(LV_SYMBOL_PLAY, "Emergency", [] { plat::playSound(core::Sound::Emergency); }, pal().text);
}

void pageFavorites() {
  const Palette& p = pal();
  const std::vector<core::Favorite> favs = app::favorites();
  std::vector<core::Track> tracks;
  app::traffic().snapshot(tracks);

  if (favs.empty()) {
    lv_obj_t* l = label(s.list,
                        "No favorites yet.\n\nTap an aircraft on the radar, then the\nstar on its info card to save it.",
                        font(FontSize::M), p.text_dim);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(l, lv_pct(100));
    lv_obj_set_style_pad_top(l, sc(30), 0);
    return;
  }

  label(s.list, fmtStr("%u of %u saved", static_cast<unsigned>(favs.size()),
                       static_cast<unsigned>(core::kMaxFavorites)).c_str(),
        font(FontSize::S), p.text_dim);
  const auto& st = app::settings();
  const core::LatLon home{st.home_lat, st.home_lon};
  for (const core::Favorite& f : favs) {
    lv_obj_t* r = baseRow(s.list);
    lv_obj_t* star = lv_image_create(r);
    lv_image_set_src(star, sprites::star(sprites::starButtonSize()));
    lv_obj_set_style_image_recolor(star, p.favorite, 0);
    lv_obj_set_style_image_recolor_opa(star, LV_OPA_COVER, 0);

    std::string status = "not in range";
    for (const core::Track& t : tracks) {
      if (std::strcmp(t.ac.hex, f.hex) == 0) {
        char d[20];
        app::formatDistance(core::distanceKm(home, core::positionAt(t, plat::millis())), d, sizeof(d));
        status = std::string("in range, ") + d;
        break;
      }
    }
    lv_obj_t* col = lv_obj_create(r);
    lv_obj_remove_style_all(col);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    label(col, f.label, font(FontSize::M), p.text);
    label(col, fmtStr("%s  %s  -  %s", f.type[0] ? f.type : "?", f.hex, status.c_str()).c_str(),
          font(FontSize::S), p.text_dim);

    const std::string hex = f.hex, name = f.label;
    lv_obj_t* del = iconButton(r, LV_SYMBOL_TRASH, sc(56), nullptr);
    onClick(del, [hex, name] {
      confirm("Remove favorite?", (name + " will no longer be highlighted or announced.").c_str(), "Remove",
              [hex] {
                app::removeFavorite(hex.c_str());
                RadarView::get().markStaticDirty();
                showPage(Page::Favorites);
              });
    });
  }
}

/** Filters apply from the next fetch; clear the screen so the change shows at once. */
void refilter() {
  app::traffic().clear();
  app::requestRefresh();
}

/** Switch bound to one bit of a mask. */
void maskSwitchRow(const char* text, uint8_t* mask, uint8_t bit) {
  const Palette& p = pal();
  lv_obj_t* r = baseRow(s.list);
  growLabel(r, text, p.text);
  lv_obj_t* sw = lv_switch_create(r);
  lv_obj_set_size(sw, sc(92), sc(48));
  lv_obj_set_style_bg_color(sw, p.accent, static_cast<lv_style_selector_t>(LV_PART_INDICATOR) | LV_STATE_CHECKED);
  if (*mask & bit) lv_obj_add_state(sw, LV_STATE_CHECKED);
  onEvent(sw, LV_EVENT_VALUE_CHANGED, [sw, mask, bit] {
    if (lv_obj_has_state(sw, LV_STATE_CHECKED)) *mask |= bit;
    else *mask &= static_cast<uint8_t>(~bit);
    app::sound(core::Sound::Click);
    changed();
    refilter();
  });
}

void pageFilters() {
  auto& st = app::settings();
  label(s.list, "Favorites are always shown, whatever the filters.", font(FontSize::S), pal().text_dim);

  section("AIRCRAFT KINDS");
  for (int i = 0; i < core::kCategoryCount; ++i) {
    const auto bit = static_cast<core::CategoryBit>(1 << i);
    maskSwitchRow(core::categoryName(bit), &st.category_mask, bit);
  }
  switchRow("Military aircraft only", &st.military_only, [] { refilter(); });
  switchRow("Aircraft on the ground", &st.show_ground, [] { refilter(); });

  section("TYPE");
  const std::string types = st.type_filter[0] ? st.type_filter : "All types";
  lv_obj_t* tr = actionRow(LV_SYMBOL_KEYBOARD, ("Only types: " + types).c_str(), [] {
    textInput("Types, e.g. B738, A32*, C17 (empty = all)", app::settings().type_filter, InputKind::Text,
              [](const std::string& v) {
                auto& st = app::settings();
                std::string up;
                for (char c : v) up += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                std::snprintf(st.type_filter, sizeof(st.type_filter), "%s", up.c_str());
                changed();
                refilter();
                showPage(Page::Filters);
              });
  }, pal().text);
  (void)tr;
  label(s.list, "ICAO type codes; * matches a prefix (A32* = A320 family).", font(FontSize::S), pal().text_dim);

  section("ALTITUDE");
  sliderRow("Hide below", 0, 80, st.min_alt_ft / 500, [](int v) { app::settings().min_alt_ft = v * 500; },
            [](int v) { return v == 0 ? std::string("No limit") : fmtStr("%d ft", v * 500); },
            [] { refilter(); });
  sliderRow("Hide above", 2, 120, st.max_alt_ft / 500, [](int v) { app::settings().max_alt_ft = v * 500; },
            [](int v) { return v >= 120 ? std::string("No limit") : fmtStr("%d ft", v * 500); },
            [] { refilter(); });

  section("GROUND SPEED");
  sliderRow("Slower than", 0, 60, st.min_speed_kt / 10,
            [](int v) { app::settings().min_speed_kt = static_cast<uint16_t>(v * 10); },
            [](int v) { return v == 0 ? std::string("Show all") : fmtStr("hide < %d kt", v * 10); },
            [] { refilter(); });
  sliderRow("Faster than", 5, 100, std::min<int>(st.max_speed_kt, core::kSpeedNoLimit) / 10,
            [](int v) { app::settings().max_speed_kt = static_cast<uint16_t>(v * 10); },
            [](int v) { return v * 10 >= core::kSpeedNoLimit ? std::string("Show all") : fmtStr("hide > %d kt", v * 10); },
            [] { refilter(); });

  section("");
  actionRow(LV_SYMBOL_REFRESH, "Reset all filters", [] {
    auto& st = app::settings();
    const core::Settings d;
    st.category_mask = d.category_mask;
    st.military_only = d.military_only;
    st.show_ground = d.show_ground;
    st.min_alt_ft = d.min_alt_ft;
    st.max_alt_ft = d.max_alt_ft;
    st.min_speed_kt = d.min_speed_kt;
    st.max_speed_kt = d.max_speed_kt;
    st.type_filter[0] = '\0';
    changed();
    refilter();
    showPage(Page::Filters);
  }, pal().warn);
}

void pageTraffic() {
  auto& st = app::settings();
  section("DATA SOURCE");
  static const char* const kSources[] = {"adsb.fi", "airplanes.live", "adsb.lol", ""};
  choiceRow("ADS-B feed", kSources, static_cast<int>(st.source), [](int v) {
    app::settings().source = static_cast<core::DataSource>(v);
    app::requestRefresh();
  });
  sliderRow("Update every", 2, 30, st.poll_interval_s,
            [](int v) { app::settings().poll_interval_s = static_cast<uint8_t>(v); },
            [](int v) { return fmtStr("%d s", v); });
  label(s.list,
        "Free community feeds. Please keep the interval\nat 5 s or more for everyday use.",
        font(FontSize::S), pal().text_dim);
  actionRow(LV_SYMBOL_LIST, "Filters...", [] { showPage(Page::Filters); }, pal().accent);
}

void pageAbout() {
  lv_obj_t* fw = infoRow("Firmware", plat::firmwareVersion());
  (void)fw;
  infoRow("Display", fmtStr("%d x %d", static_cast<int>(screenW()), static_cast<int>(screenH())));
  lv_obj_t* wifi = infoRow("Wi-Fi", "");
  lv_obj_t* feed = infoRow("Feed", "");
  lv_obj_t* mem = infoRow("Free memory", "");

  section("MAINTENANCE");
  actionRow(LV_SYMBOL_REFRESH, "Restart", [] {
    app::settingsFlush();
    plat::restart();
  }, pal().text);
  actionRow(LV_SYMBOL_TRASH, "Factory reset", [] {
    confirm("Factory reset?", "Erases Wi-Fi, location and all settings, then restarts.", "Erase", [] {
      plat::settingsErase();
      plat::restart();
    });
  }, pal().danger);
  label(s.list, "Plane Radar P4 - aircraft data from community\nADS-B feeds; runways from OurAirports.",
        font(FontSize::S), pal().text_dim);

  s.tick = [wifi, feed, mem] {
    const plat::WifiStatus ws = plat::wifiStatus();
    lv_label_set_text(wifi, ws.state == plat::WifiState::Connected
                                ? fmtStr("%s, %d dBm", ws.ip, ws.rssi).c_str()
                                : "offline");
    const app::FeedStatus fs = app::feedStatus();
    if (fs.ever_ok) {
      const unsigned age = (plat::millis() - fs.last_ok_ms) / 1000;
      lv_label_set_text(feed, fmtStr("%d aircraft, %us ago", fs.last_count, age).c_str());
    } else {
      lv_label_set_text(feed, fs.error[0] ? fs.error : "waiting");
    }
    lv_label_set_text(mem, fmtStr("%u KB RAM, %u KB PSRAM", static_cast<unsigned>(plat::freeInternalHeap() / 1024),
                                  static_cast<unsigned>(plat::freePsram() / 1024))
                               .c_str());
  };
}

const char* pageTitle(Page p) {
  switch (p) {
    case Page::Wifi: return "Wi-Fi";
    case Page::Location: return "Location";
    case Page::Display: return "Display";
    case Page::Sound: return "Sound";
    case Page::Traffic: return "Traffic & data";
    case Page::Filters: return "Filters";
    case Page::Favorites: return "Favorites";
    case Page::About: return "About";
    default: return "Settings";
  }
}

void showPage(Page page) {
  s.page = page;
  s.tick = nullptr;
  s.wifi_status = nullptr;
  s.wifi_list = nullptr;
  s.scanning = false;
  lv_label_set_text(s.title, pageTitle(page));
  lv_obj_clean(s.list);
  lv_obj_scroll_to_y(s.list, 0, LV_ANIM_OFF);
  switch (page) {
    case Page::Wifi: pageWifi(); break;
    case Page::Location: pageLocation(); break;
    case Page::Display: pageDisplay(); break;
    case Page::Sound: pageSound(); break;
    case Page::Traffic: pageTraffic(); break;
    case Page::Filters: pageFilters(); break;
    case Page::Favorites: pageFavorites(); break;
    case Page::About: pageAbout(); break;
    default: pageMain(); break;
  }
  if (s.tick) s.tick();
}

void onTimer(lv_timer_t*) {
  if (s.tick) s.tick();
}

}  // namespace

lv_obj_t* create(Page page) {
  const Palette& p = pal();
  s = State{};
  s.screen = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(s.screen, p.bg, 0);
  lv_obj_set_style_bg_opa(s.screen, LV_OPA_COVER, 0);
  lv_obj_remove_flag(s.screen, LV_OBJ_FLAG_SCROLLABLE);

  s.title = label(s.screen, "", font(FontSize::XL), p.text);
  lv_obj_align(s.title, LV_ALIGN_TOP_MID, 0, sc(68));

  // Back (left) and close (right) on the upper diagonals of the circle.
  lv_obj_t* back = iconButton(s.screen, LV_SYMBOL_LEFT, sc(72), [] {
    if (s.page == Page::Main) closeSettings();
    else showPage(Page::Main);
  });
  lv_obj_align(back, LV_ALIGN_CENTER, -sc(196), -sc(286));
  lv_obj_t* close = iconButton(s.screen, LV_SYMBOL_CLOSE, sc(72), [] { closeSettings(); });
  lv_obj_align(close, LV_ALIGN_CENTER, sc(196), -sc(286));

  // Scrolling list; generous bottom padding lets the last rows scroll up into
  // the wide middle band of the round panel.
  s.list = lv_obj_create(s.screen);
  lv_obj_remove_style_all(s.list);
  lv_obj_set_size(s.list, sc(600), screenH() - sc(160));
  lv_obj_align(s.list, LV_ALIGN_TOP_MID, 0, sc(156));
  lv_obj_set_flex_flow(s.list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(s.list, sc(12), 0);
  lv_obj_set_style_pad_top(s.list, sc(8), 0);
  lv_obj_set_style_pad_bottom(s.list, sc(220), 0);
  lv_obj_set_scrollbar_mode(s.list, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_scroll_dir(s.list, LV_DIR_VER);

  // The buttons sit inside the round edge, just above the list; keep them on top.
  lv_obj_move_foreground(back);
  lv_obj_move_foreground(close);

  s.timer = lv_timer_create(onTimer, 500, nullptr);
  showPage(page);
  return s.screen;
}

void destroy() {
  if (s.timer) lv_timer_delete(s.timer);
  s.timer = nullptr;
  s.tick = nullptr;
}

}  // namespace ui::settings_ui
