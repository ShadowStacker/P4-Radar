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
#include "app/maps.h"
#include "app/web.h"
#include "core/adsb.h"
#include "core/clock.h"
#include "core/daylog.h"
#include "core/filter.h"
#include "core/tiles.h"
#include "core/watch.h"
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
  // Alarm being edited
  int alarm_index = 0;
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

/** navRow with an image icon (for icons the fonts don't have). */
lv_obj_t* navRowImage(const lv_draw_buf_t* icon, const char* text, const std::string& value, Page target) {
  lv_obj_t* r = navRow("", text, value, target);
  lv_obj_t* img = lv_image_create(r);
  lv_image_set_src(img, icon);
  lv_obj_set_style_image_recolor(img, pal().accent, 0);
  lv_obj_set_style_image_recolor_opa(img, LV_OPA_COVER, 0);
  lv_obj_move_to_index(img, 0);
  lv_obj_delete(lv_obj_get_child(r, 1));  // the empty symbol label
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
  int rows = 1;  // "\n" entries start another row (they don't count as buttons)
  for (const char* const* m = map; **m != '\0'; ++m) rows += std::strcmp(*m, "\n") == 0;
  lv_obj_set_size(bm, lv_pct(100), sc(64) * rows + sc(6) * (rows - 1));
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

std::string updateSummary();

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
  {
    char next[32] = "";
    for (const core::Alarm& a : st.alarms) {
      if (!a.enabled) continue;
      core::formatTime(a.hour, a.minute, st.clock_24h, next, sizeof(next));
      break;
    }
    navRowImage(sprites::clockIcon(sc(30) | 1), "Clock & alarms", next[0] ? fmtStr(LV_SYMBOL_BELL " %s", next) : "",
                Page::Clock);
  }
  {
    const app::maps::SdStatus sd = app::maps::sdStatus();
    navRow(LV_SYMBOL_SD_CARD, "SD card",
           sd.state == app::maps::SdState::Mounted ? fmtStr("%.1f GB free", sd.free_bytes / 1e9)
                                                   : std::string(sd.state == app::maps::SdState::Unknown ? "..." : "No card"),
           Page::SdCard);
  }
  navRowImage(sprites::star(sc(30) | 1), "Favorites", fmtStr("%u", static_cast<unsigned>(app::favorites().size())),
              Page::Favorites);
  navRow(LV_SYMBOL_BELL, "Watchlist alerts", core::watchActive(st) ? "On" : "Off", Page::Watchlist);
  navRow(LV_SYMBOL_CALL, "Phone settings page", st.web_enabled ? app::web::address() : std::string("Off"),
         Page::Phone);
  navRow(LV_SYMBOL_LIST, "Daily log", fmtStr("%d aircraft today", app::dayLogToday().count()), Page::DayLog);
  navRow(LV_SYMBOL_DOWNLOAD, "Updates", updateSummary(), Page::Updates);
  navRow(LV_SYMBOL_LIST, "About", plat::firmwareVersion(), Page::About);
}

void connectTo(const std::string& ssid, const std::string& pass) {
  app::useWifi(ssid.c_str(), pass.c_str());  // "Connecting to ..." is shown by the UI's Wi-Fi watcher
  if (!app::settings().home_set) {
    // First-time setup continues with the location.
    toast((std::string(LV_SYMBOL_WIFI "  Connecting... now set your home location")).c_str(), 3500);
    showPage(Page::Location);
  } else if (s.page == Page::Wifi) {
    showPage(Page::Wifi);  // the saved list changed order
  }
}

void askPassword(const std::string& ssid) {
  const std::string title = "Password for " + ssid;
  std::string initial;
  app::isSavedWifi(ssid.c_str(), &initial);
  textInput(title.c_str(), initial, InputKind::Password,
            [ssid](const std::string& pass) { connectTo(ssid, pass); });
}

/** Saved networks: tap to connect, trash to forget. */
void fillSavedWifi() {
  const Palette& p = pal();
  const std::vector<core::SavedWifi> saved = app::savedWifi();
  const plat::WifiStatus ws = plat::wifiStatus();
  if (saved.empty()) {
    label(s.list, "None yet - pick a network below", font(FontSize::S), p.text_dim);
    return;
  }
  for (size_t i = 0; i < saved.size(); ++i) {
    const core::SavedWifi w = saved[i];
    lv_obj_t* r = baseRow(s.list);
    lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(r, p.panel_border, LV_STATE_PRESSED);
    const bool connected = ws.state == plat::WifiState::Connected && std::strcmp(ws.ssid, w.ssid) == 0;
    label(r, connected ? LV_SYMBOL_OK : LV_SYMBOL_WIFI, font(FontSize::M), connected ? p.accent : p.text_dim);
    growLabel(r, w.ssid, p.text);
    if (i == 0) label(r, "first", font(FontSize::S), p.text_dim);
    onClick(r, [w] { connectTo(w.ssid, w.pass); });
    lv_obj_t* del = iconButton(r, LV_SYMBOL_TRASH, sc(52), [w] {
      confirm((std::string("Forget ") + w.ssid + "?").c_str(),
              "The radar won't connect to it anymore (until you enter its password again).", "Forget", [w] {
        app::forgetWifi(w.ssid);
        toast((std::string(LV_SYMBOL_TRASH "  Forgot ") + w.ssid).c_str());
        showPage(Page::Wifi);
      });
    });
    lv_obj_set_style_text_color(lv_obj_get_child(del, 0), p.danger, 0);
  }
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
    std::string saved_pass;
    const bool saved = app::isSavedWifi(ssid.c_str(), &saved_pass);
    if (saved) label(r, "saved", font(FontSize::S), p.text_dim);
    onClick(r, [ssid, secure, saved, saved_pass] {
      if (saved) connectTo(ssid, saved_pass);  // no need to type it again
      else if (secure) askPassword(ssid);
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
  section("SAVED NETWORKS");
  fillSavedWifi();
  lv_obj_t* note = label(s.list,
                         "Up to five. When one can't connect for 20 seconds, the radar tries the next.",
                         font(FontSize::S), pal().text_dim);
  lv_obj_set_width(note, lv_pct(90));
  lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_pad_left(note, sc(16), 0);
  section("NETWORKS NEARBY");
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
  static const char* const kThemes[] = {"Green", "Amber", "Blue", "Night", "Custom", ""};
  choiceRow("Theme", kThemes, static_cast<int>(st.theme), [](int v) {
    app::settings().theme = static_cast<core::Theme>(v);
    app::settingsChanged();
    lv_async_call([](void*) { rebuild(); }, nullptr);  // rebuild every screen with new colors
  });
  if (st.theme == core::Theme::Custom) {
    navRow(LV_SYMBOL_EDIT, "Custom colors", "", Page::CustomTheme);
  }
  sliderRow("Brightness", 5, 100, st.brightness, [](int v) { app::settings().brightness = static_cast<uint8_t>(v); },
            [](int v) { return fmtStr("%d%%", v); });
  static const char* const kUnits[] = {"km", "mi", "nm", ""};
  choiceRow("Distance units", kUnits, static_cast<int>(st.units),
            [](int v) { app::settings().units = static_cast<core::Units>(v); });

  static const char* const kRotations[] = {"0" "\xC2\xB0", "90" "\xC2\xB0", "180" "\xC2\xB0", "270" "\xC2\xB0", ""};
  choiceRow("Screen rotation (clockwise)", kRotations, st.rotation & 3, [](int v) {
    if (app::settings().rotation == v) return;
    app::settings().rotation = static_cast<uint8_t>(v);
    app::settingsChanged();
    app::settingsFlush();
    confirm("Restart to rotate?",
            "The new orientation takes effect when the radar restarts. It takes about 5 seconds.",
            LV_SYMBOL_REFRESH " Restart now", [] { plat::restart(); });
  });

  switchRow("Auto-hide buttons after 10 s", &st.autohide_buttons);

  section("AIRCRAFT");
  static const char* const kLabels[] = {"Off", "Callsign", "Full", ""};
  choiceRow("Labels", kLabels, static_cast<int>(st.labels),
            [](int v) { app::settings().labels = static_cast<core::LabelMode>(v); });
  static const char* const kColors[] = {"Theme", "By altitude", ""};
  choiceRow("Aircraft colors", kColors, static_cast<int>(st.color_mode),
            [](int v) { app::settings().color_mode = static_cast<core::ColorMode>(v); });
  sliderRow("Aircraft size", 6, 16, st.aircraft_scale / 10,
            [](int v) { app::settings().aircraft_scale = static_cast<uint8_t>(v * 10); },
            [](int v) { return fmtStr("%d%%", v * 10); },
            [] { sprites::init(app::settings().aircraft_scale); });
  static const char* const kLabelSizes[] = {"Small", "Normal", "Large", ""};
  choiceRow("Label size", kLabelSizes, st.label_size > 2 ? 1 : st.label_size, [](int v) {
    app::settings().label_size = static_cast<uint8_t>(v);
    setTheme(app::settings());
  });
  switchRow("Trails", &st.show_trails);
  switchRow("Flight history on tap", &st.extended_trail);
  switchRow("Airline and route on tap", &st.show_route_info);
  switchRow("Speed vectors (1 min)", &st.show_speed_vectors);

  section("MAP");
  switchRow("Range rings", &st.show_rings);
  switchRow("Runways", &st.show_runways);
  switchRow("Airport codes", &st.show_airport_labels);
  static const char* const kMaps[] = {"Off", "Dark streets", "Topo", "\n", "FAA", "Satellite", "Street map", ""};
  choiceRow("Background map", kMaps, st.map_style >= core::tiles::kStyleCount ? 0 : st.map_style,
            [](int v) { app::settings().map_style = static_cast<uint8_t>(v); });
  {
    const char* src = app::maps::cartoKeySource();
    const std::string key_text = src[0] ? fmtStr("CARTO key: set (%s)", src) : std::string("CARTO key: not set");
    actionRow(LV_SYMBOL_EDIT, key_text.c_str(), [] {
      textInput("CARTO API key (free at carto.com/basemaps)", app::settings().carto_key, InputKind::Text,
                [](const std::string& text) {
                  std::string key;
                  for (char c : text) {
                    if (c != ' ') key.push_back(c);
                  }
                  std::snprintf(app::settings().carto_key, sizeof(app::settings().carto_key), "%s", key.c_str());
                  changed();
                  showPage(Page::Display);
                });
    }, pal().accent);
    lv_obj_t* note = label(s.list,
                           "Dark streets uses CARTO's map with a free API key (or a carto_key.txt file on the SD "
                           "card). Without one it shows OpenStreetMap's standard map, darkened.",
                           font(FontSize::S), pal().text_dim);
    lv_obj_set_width(note, lv_pct(90));
    lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_pad_left(note, sc(16), 0);
  }
  switchRow("Place names (dark streets with a CARTO key)", &st.map_labels);
  switchRow("Tint map to theme color", &st.map_tint);
  sliderRow("Map brightness", 10, 100, st.map_brightness,
            [](int v) { app::settings().map_brightness = static_cast<uint8_t>(v); },
            [](int v) { return fmtStr("%d%%", v); });
  navRow(LV_SYMBOL_SD_CARD, "SD card", "", Page::SdCard);
  static const char* const kCodeStyles[] = {"ICAO (KRDM)", "IATA (RDM)", ""};
  choiceRow("Airport code style", kCodeStyles, st.airport_codes > 1 ? 0 : st.airport_codes,
            [](int v) { app::settings().airport_codes = static_cast<uint8_t>(v); });

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

  // Night mode, by the clock (in 15-minute steps): dim, or sleep.
  switchRow("Night mode", &st.night_dim);
  static const char* const kNightModes[] = {"Dim the screen", "Sleep", ""};
  choiceRow("At night", kNightModes, st.night_sleep ? 1 : 0,
            [](int v) { app::settings().night_sleep = v == 1; });
  static const char* const kNightBy[] = {"Set times", "Sunset to sunrise", ""};
  choiceRow("Night is", kNightBy, st.night_by_sun ? 1 : 0, [](int v) {
    app::settings().night_by_sun = v == 1;
    lv_async_call([](void*) { showPage(Page::Display); }, nullptr);  // the time sliders come and go
  });
  if (st.night_by_sun) {
    int set = 0, rise = 0;
    bool dark = false;
    std::string when;
    if (app::sunTimesToday(&set, &rise, &dark)) {
      char a[16], b[16];
      core::formatTime(set / 60, set % 60, st.clock_24h, a, sizeof(a));
      core::formatTime(rise / 60, rise % 60, st.clock_24h, b, sizeof(b));
      when = fmtStr("sunset %s, sunrise %s", a, b);
    } else {
      when = plat::timeValid() ? (dark ? "no sunrise today" : "no sunset today") : "waiting for the time";
    }
    lv_obj_t* l = infoRow("Today at home", when);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  } else {
    auto timeText = [](int quarter) {
      char buf[16];
      core::formatTime(quarter / 4, quarter % 4 * 15, app::settings().clock_24h, buf, sizeof(buf));
      return std::string(buf);
    };
    sliderRow("Night starts", 0, 95, st.night_from_min / 15,
              [](int v) { app::settings().night_from_min = static_cast<uint16_t>(v * 15); }, timeText);
    sliderRow("Night ends", 0, 95, st.night_to_min / 15,
              [](int v) { app::settings().night_to_min = static_cast<uint16_t>(v * 15); }, timeText);
  }
  sliderRow("Night brightness (when dimming)", 1, 100, st.night_brightness,
            [](int v) { app::settings().night_brightness = static_cast<uint8_t>(v); },
            [](int v) { return fmtStr("%d%%", v); });
}

// Custom theme: live swatches on this page, the whole UI re-themes when a
// slider is let go.
lv_obj_t* s_swatch[4] = {};

void refreshSwatches() {
  const Palette c = customPalette(app::settings());
  const lv_color_t colors[4] = {c.accent, c.bg_center, c.aircraft, c.label};
  for (int i = 0; i < 4; ++i) {
    if (s_swatch[i]) lv_obj_set_style_bg_color(s_swatch[i], colors[i], 0);
  }
}

void applyCustom() {
  refreshSwatches();
  lv_async_call([](void*) { rebuild(); }, nullptr);
}

void hueSliders(const char* name, uint16_t* hue, uint8_t* sat, const char* sat_zero) {
  sliderRow(fmtStr("%s - hue", name).c_str(), 0, 359, *hue, [hue](int v) {
    *hue = static_cast<uint16_t>(v);
    refreshSwatches();
  }, [](int v) { return fmtStr("%d" "\xC2\xB0", v); }, applyCustom);
  sliderRow(fmtStr("%s - saturation", name).c_str(), 0, 100, *sat, [sat](int v) {
    *sat = static_cast<uint8_t>(v);
    refreshSwatches();
  }, [sat_zero](int v) { return v == 0 && sat_zero ? std::string(sat_zero) : fmtStr("%d%%", v); }, applyCustom);
}

void pageCustomTheme() {
  auto& st = app::settings();
  const Palette& p = pal();

  // Preview: radar color, background, aircraft, labels.
  lv_obj_t* r = baseRow(s.list);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  static const char* const kNames[] = {"Radar", "Background", "Aircraft", "Labels"};
  for (int i = 0; i < 4; ++i) {
    lv_obj_t* col = lv_obj_create(r);
    lv_obj_remove_style_all(col);
    lv_obj_set_size(col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, sc(6), 0);
    s_swatch[i] = lv_obj_create(col);
    lv_obj_remove_style_all(s_swatch[i]);
    lv_obj_set_size(s_swatch[i], sc(54), sc(54));
    lv_obj_set_style_radius(s_swatch[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_swatch[i], LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_swatch[i], p.text_dim, 0);
    lv_obj_set_style_border_width(s_swatch[i], sc(2), 0);
    label(col, kNames[i], font(FontSize::S), p.text_dim);
  }
  lv_obj_add_event_cb(r, [](lv_event_t*) { std::fill(std::begin(s_swatch), std::end(s_swatch), nullptr); },
                      LV_EVENT_DELETE, nullptr);
  refreshSwatches();

  section("RADAR");
  hueSliders("Radar color", &st.custom_hue, &st.custom_sat, "Gray");
  sliderRow("Background brightness", 0, 30, st.custom_bg, [](int v) {
    app::settings().custom_bg = static_cast<uint8_t>(v);
    refreshSwatches();
  }, [](int v) { return v == 0 ? std::string("Black") : fmtStr("%d%%", v); }, applyCustom);
  sliderRow("Rings and compass brightness", 15, 100, st.custom_grid,
            [](int v) { app::settings().custom_grid = static_cast<uint8_t>(v); },
            [](int v) { return fmtStr("%d%%", v); }, applyCustom);

  section("AIRCRAFT");
  hueSliders("Aircraft color", &st.custom_aircraft_hue, &st.custom_aircraft_sat, "White");
  {
    lv_obj_t* note = label(s.list, "Used when Display > Aircraft colors is set to Theme.", font(FontSize::S),
                           pal().text_dim);
    lv_obj_set_width(note, lv_pct(90));
    lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_pad_left(note, sc(16), 0);
  }
  hueSliders("Label color", &st.custom_label_hue, &st.custom_label_sat, "White");

  section("ALTITUDE COLORS");
  {
    lv_obj_t* note = label(s.list, "Used when Display > Aircraft colors is set to By altitude.", font(FontSize::S),
                           p.text_dim);
    lv_obj_set_width(note, lv_pct(90));
    lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_pad_left(note, sc(16), 0);
  }
  struct AltHue {
    const char* name;
    uint16_t* hue;
  };
  const AltHue alts[] = {{"Low (ground) - hue", &st.custom_alt_low_hue},
                         {"Middle (10,000 ft) - hue", &st.custom_alt_mid_hue},
                         {"High (40,000 ft) - hue", &st.custom_alt_high_hue}};
  for (const AltHue& a : alts) {
    uint16_t* hue = a.hue;
    sliderRow(a.name, 0, 359, *hue, [hue](int v) { *hue = static_cast<uint16_t>(v); },
              [](int v) { return fmtStr("%d" "\xC2\xB0", v); }, applyCustom);
  }

  actionRow(LV_SYMBOL_REFRESH, "Reset custom colors", [] {
    const core::Settings d;
    auto& c = app::settings();
    c.custom_hue = d.custom_hue;
    c.custom_sat = d.custom_sat;
    c.custom_bg = d.custom_bg;
    c.custom_grid = d.custom_grid;
    c.custom_aircraft_hue = d.custom_aircraft_hue;
    c.custom_aircraft_sat = d.custom_aircraft_sat;
    c.custom_label_hue = d.custom_label_hue;
    c.custom_label_sat = d.custom_label_sat;
    c.custom_alt_low_hue = d.custom_alt_low_hue;
    c.custom_alt_mid_hue = d.custom_alt_mid_hue;
    c.custom_alt_high_hue = d.custom_alt_high_hue;
    changed();
    applyCustom();
  }, p.warn);
}

// Hue + saturation pair with a swatch in the row title (clock hand colors).
void colorSliders(const char* name, uint16_t* hue, uint8_t* sat, const char* sat_zero) {
  const Palette& p = pal();
  lv_obj_t* r = baseRow(s.list);
  growLabel(r, name, p.text);
  lv_obj_t* sw = lv_obj_create(r);
  lv_obj_remove_style_all(sw);
  lv_obj_set_size(sw, sc(40), sc(40));
  lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(sw, p.text_dim, 0);
  lv_obj_set_style_border_width(sw, sc(2), 0);
  auto paint = [sw, hue, sat] {
    lv_obj_set_style_bg_color(sw, lv_color_hsv_to_rgb(*hue % 360, std::min<int>(*sat, 100), 100), 0);
  };
  paint();
  sliderRow("Hue", 0, 359, *hue, [hue, paint](int v) {
    *hue = static_cast<uint16_t>(v);
    paint();
  }, [](int v) { return fmtStr("%d" "\xC2\xB0", v); });
  sliderRow("Saturation", 0, 100, *sat, [sat, paint](int v) {
    *sat = static_cast<uint8_t>(v);
    paint();
  }, [sat_zero](int v) { return v == 0 ? std::string(sat_zero) : fmtStr("%d%%", v); });
}

// ---------------------------------------------------------------------------
// Clock & alarms

std::string nowText() {
  if (!plat::timeValid()) return "Not set yet (needs Wi-Fi)";
  const auto& st = app::settings();
  const int64_t now = plat::unixTimeMs() / 1000;
  const std::tm t = core::localTime(now);
  char hm[16];
  core::formatTime(t.tm_hour, t.tm_min, st.clock_24h, hm, sizeof(hm));
  static const char* const kDays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  static const char* const kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  return fmtStr("%s, %s %s %d", hm, kDays[t.tm_wday % 7], kMonths[t.tm_mon % 12], t.tm_mday);
}

void pageClock() {
  auto& st = app::settings();
  const Palette& p = pal();
  lv_obj_t* now = infoRow("Time now", nowText());
  s.tick = [now] { lv_label_set_text(now, nowText().c_str()); };
  navRow(LV_SYMBOL_GPS, "Time zone", core::timeZone(st.time_zone).name, Page::TimeZone);
  switchRow("Clock face on the radar", &st.clock_mode);
  switchRow("24-hour time", &st.clock_24h);
  switchRow("Seconds hand", &st.clock_seconds);
  switchRow("Smooth seconds hand", &st.clock_smooth_seconds);
  switchRow("Date window", &st.clock_show_date);

  section("HAND COLORS");
  colorSliders("Hour and minute hands", &st.clock_hand_hue, &st.clock_hand_sat, "White");
  colorSliders("Seconds hand", &st.clock_second_hue, &st.clock_second_sat, "White");

  section("ALARMS");
  for (int i = 0; i < core::kMaxAlarms; ++i) {
    core::Alarm& a = st.alarms[i];
    lv_obj_t* r = baseRow(s.list);
    lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(r, p.panel_border, LV_STATE_PRESSED);
    lv_obj_t* col = lv_obj_create(r);
    lv_obj_remove_style_all(col);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(col, LV_OBJ_FLAG_CLICKABLE);
    char hm[16], days[40];
    core::formatTime(a.hour, a.minute, st.clock_24h, hm, sizeof(hm));
    core::formatDays(a.days, days, sizeof(days));
    label(col, hm, font(FontSize::L), a.enabled ? p.text : p.text_dim);
    label(col, days, font(FontSize::S), p.text_dim);
    lv_obj_t* sw = lv_switch_create(r);
    lv_obj_set_size(sw, sc(92), sc(48));
    lv_obj_set_style_bg_color(sw, p.accent, static_cast<lv_style_selector_t>(LV_PART_INDICATOR) | LV_STATE_CHECKED);
    if (a.enabled) lv_obj_add_state(sw, LV_STATE_CHECKED);
    onEvent(sw, LV_EVENT_VALUE_CHANGED, [sw, i] {
      app::settings().alarms[i].enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
      app::sound(core::Sound::Click);
      changed();
      lv_async_call([](void*) { showPage(Page::Clock); }, nullptr);  // refresh the time's color
    });
    onClick(r, [i] {
      s.alarm_index = i;
      showPage(Page::AlarmEdit);
    });
  }
  switchRow("Wake on alarm (when asleep)", &st.wake_on_alarm);

  const int snooze = app::snoozeMinutesLeft();
  if (snooze >= 0) {
    infoRow("Snoozed", fmtStr("rings again in %d min", snooze));
    actionRow(LV_SYMBOL_CLOSE, "Cancel snooze", [] {
      app::stopAlarm();
      showPage(Page::Clock);
    }, p.warn);
  }
  lv_obj_t* note = label(s.list,
                         "Tap an alarm to set its time and days. Alarms ring even when sound is off. While the "
                         "screen is asleep they ring and wake it only if Wake on alarm is on. "
                         "The clock is set from the internet, so it needs Wi-Fi after power-up.",
                         font(FontSize::S), p.text_dim);
  lv_obj_set_width(note, lv_pct(90));
  lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_pad_left(note, sc(16), 0);
}

void pageTimeZone() {
  const auto& st = app::settings();
  const Palette& p = pal();
  for (int i = 0; i < core::timeZoneCount(); ++i) {
    lv_obj_t* r = baseRow(s.list);
    lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(r, p.panel_border, LV_STATE_PRESSED);
    growLabel(r, core::timeZone(i).name, i == st.time_zone ? p.accent : p.text);
    if (i == st.time_zone) label(r, LV_SYMBOL_OK, font(FontSize::M), p.accent);
    onClick(r, [i] {
      app::settings().time_zone = static_cast<uint8_t>(i);
      app::timeZoneChanged();
      app::sound(core::Sound::Click);
      changed();
      showPage(Page::Clock);
    });
  }
}

void pageAlarmEdit() {
  auto& st = app::settings();
  const Palette& p = pal();
  const int i = s.alarm_index;
  core::Alarm& a = st.alarms[i];

  lv_obj_t* big = label(s.list, "", font(FontSize::XXL), p.text);
  lv_obj_set_width(big, lv_pct(100));
  lv_obj_set_style_text_align(big, LV_TEXT_ALIGN_CENTER, 0);
  static lv_obj_t* s_big = nullptr;
  s_big = big;
  auto refresh = [] {
    const auto& c = app::settings();
    const core::Alarm& al = c.alarms[s.alarm_index];
    char hm[16];
    core::formatTime(al.hour, al.minute, c.clock_24h, hm, sizeof(hm));
    if (s_big) lv_label_set_text(s_big, hm);
  };
  refresh();
  lv_obj_add_event_cb(big, [](lv_event_t*) { s_big = nullptr; }, LV_EVENT_DELETE, nullptr);

  switchRow("Alarm on", &a.enabled);
  // Changing the time switches the alarm on: that's nearly always the intent.
  sliderRow("Hour", 0, 23, a.hour, [refresh](int v) {
    auto& al = app::settings().alarms[s.alarm_index];
    al.hour = static_cast<uint8_t>(v);
    al.enabled = true;
    refresh();
  }, [](int v) {
    char hm[16];
    core::formatTime(v, 0, app::settings().clock_24h, hm, sizeof(hm));
    return app::settings().clock_24h ? fmtStr("%02d", v) : std::string(hm).replace(std::string(hm).find(":00"), 3, "");
  });
  sliderRow("Minute", 0, 59, a.minute, [refresh](int v) {
    auto& al = app::settings().alarms[s.alarm_index];
    al.minute = static_cast<uint8_t>(v);
    al.enabled = true;
    refresh();
  }, [](int v) { return fmtStr(":%02d", v); });

  // Days: Monday first; none selected = rings once.
  {
    lv_obj_t* r = baseRow(s.list);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(r, sc(10), 0);
    char days[40];
    core::formatDays(a.days, days, sizeof(days));
    lv_obj_t* title = label(r, fmtStr("Repeat: %s", days).c_str(), font(FontSize::M), p.text);
    static const char* const kMap[] = {"Mo", "Tu", "We", "Th", "Fr", "Sa", "Su", ""};
    lv_obj_t* bm = lv_buttonmatrix_create(r);
    lv_buttonmatrix_set_map(bm, kMap);
    lv_obj_set_size(bm, lv_pct(100), sc(64));
    lv_buttonmatrix_set_button_ctrl_all(bm, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    for (int k = 0; k < 7; ++k) {
      if (a.days & (1u << ((k + 1) % 7))) lv_buttonmatrix_set_button_ctrl(bm, k, LV_BUTTONMATRIX_CTRL_CHECKED);
    }
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
    onEvent(bm, LV_EVENT_VALUE_CHANGED, [bm, title] {
      uint8_t mask = 0;
      for (int k = 0; k < 7; ++k) {
        if (lv_buttonmatrix_has_button_ctrl(bm, k, LV_BUTTONMATRIX_CTRL_CHECKED)) mask |= 1u << ((k + 1) % 7);
      }
      auto& al = app::settings().alarms[s.alarm_index];
      al.days = mask;
      al.enabled = true;
      char d[40];
      core::formatDays(mask, d, sizeof(d));
      lv_label_set_text(title, fmtStr("Repeat: %s", d).c_str());
      app::sound(core::Sound::Click);
      changed();
    });
    lv_obj_t* hint = label(r, "No days selected: rings once, then switches off.", font(FontSize::S), p.text_dim);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(hint, lv_pct(100));
  }
  actionRow(LV_SYMBOL_BELL, "Test this alarm", [] { app::testAlarm(s.alarm_index); }, p.accent);
}

// ---------------------------------------------------------------------------
// SD card

std::string bytesText(uint64_t b) {
  if (b >= 1000000000ULL) return fmtStr("%.1f GB", b / 1e9);
  if (b >= 1000000ULL) return fmtStr("%.0f MB", b / 1e6);
  return fmtStr("%.0f KB", b / 1e3);
}

void pageSdCard() {
  auto& st = app::settings();
  const Palette& p = pal();
  lv_obj_t* card = infoRow("Card", "");
  lv_obj_t* saved = infoRow("Saved map tiles", "");
  auto refresh = [card, saved] {
    const app::maps::SdStatus sd = app::maps::sdStatus();
    switch (sd.state) {
      case app::maps::SdState::Mounted:
        lv_label_set_text(card, fmtStr("%s, %s free", bytesText(sd.total_bytes).c_str(),
                                       bytesText(sd.free_bytes).c_str()).c_str());
        break;
      case app::maps::SdState::Unmounted: lv_label_set_text(card, "Unmounted - safe to remove"); break;
      case app::maps::SdState::NoCard: lv_label_set_text(card, sd.error[0] ? sd.error : "No card"); break;
      default: lv_label_set_text(card, "Checking..."); break;
    }
    if (sd.busy) lv_label_set_text(saved, "Working...");
    else if (sd.state != app::maps::SdState::Mounted) lv_label_set_text(saved, "-");
    else if (!sd.counted) lv_label_set_text(saved, "Not counted");
    else lv_label_set_text(saved, fmtStr("%u (%s)", static_cast<unsigned>(sd.tiles), bytesText(sd.tile_bytes).c_str()).c_str());
  };
  s.tick = refresh;
  const app::maps::SdStatus sd = app::maps::sdStatus();
  if (sd.state == app::maps::SdState::Mounted && !sd.counted && !sd.busy) app::maps::sdRequest(app::maps::SdAction::Count);

  switchRow("Save map tiles to the card", &st.sd_cache);
  actionRow(LV_SYMBOL_REFRESH, "Count saved tiles", [] { app::maps::sdRequest(app::maps::SdAction::Count); }, p.accent);
  actionRow(LV_SYMBOL_TRASH, "Delete saved map tiles", [] {
    confirm("Delete saved map tiles?", "Maps will download again as you look at them.", "Delete",
            [] { app::maps::sdRequest(app::maps::SdAction::Clear); });
  }, p.warn);
  switchRow("Remember airline and route lookups", &st.lookup_cache);
  actionRow(LV_SYMBOL_TRASH, "Delete saved lookups", [] {
    confirm("Delete saved lookups?", "Airlines and routes will be looked up online again.", "Delete",
            [] { app::clearLookupCache(); });
  }, p.warn);
  actionRow(LV_SYMBOL_SD_CARD, "Look for a card again", [] { app::maps::sdRequest(app::maps::SdAction::Mount); },
            p.accent);
  actionRow(LV_SYMBOL_EJECT, "Unmount (safe to remove)", [] { app::maps::sdRequest(app::maps::SdAction::Unmount); },
            p.text);
  lv_obj_t* note = label(s.list,
                         "Map tiles you look at are saved on the card, so they appear instantly next time and "
                         "work without internet for those areas. Use a FAT32-formatted microSD card. Insert it "
                         "with the power off, or tap \"Look for a card again\".\n\n"
                         "The card also keeps a copy of your settings (settings.txt, including Wi-Fi) and "
                         "favorites (favorites.txt). They are read at startup, so they come back after a "
                         "reflash, and you can edit them on a computer.\n\n"
                         "Airline and route lookups are kept in lookups/ (shown at once next time, even "
                         "offline; routes are looked up again after a month), screenshots in screenshots/ and "
                         "the daily log in log/.",
                         font(FontSize::S), p.text_dim);
  lv_obj_set_width(note, lv_pct(90));
  lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_pad_left(note, sc(16), 0);
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
  switchRow("Watchlist aircraft", &st.sound_watch);
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
  actionRow(LV_SYMBOL_PLAY, "Watchlist", [] { plat::playSound(core::Sound::Watch); }, pal().text);
}

void pageFavorites() {
  const Palette& p = pal();
  const std::vector<core::Favorite> favs = app::favorites();
  std::vector<core::Track> tracks;
  app::traffic().snapshot(tracks);

  actionRow(LV_SYMBOL_PLUS, "Add by tail number", [] {
    textInput("Tail number (e.g. N123AB) or ICAO hex", "", InputKind::Text, [](const std::string& text) {
      std::string q;
      for (char c : text) {
        if (c != ' ') q.push_back(c);
      }
      if (q.empty()) return;
      app::lookupFavorite(q);
      toast((LV_SYMBOL_REFRESH "  Looking up " + q + "...").c_str(), 2500);
    });
  }, p.accent);
  // Show a looked-up aircraft as soon as it's added.
  const size_t count = favs.size();
  s.tick = [count] {
    if (app::favorites().size() != count) showPage(Page::Favorites);
  };

  if (favs.empty()) {
    lv_obj_t* l = label(s.list,
                        "No favorites yet.\n\nTap an aircraft on the radar, then the\nstar on its info card, or add one\nby its tail number above.",
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
  label(s.list, "Favorites and watchlist aircraft are always shown.", font(FontSize::S), pal().text_dim);

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

std::string recordText(const core::daylog::Record& r, const std::string& what) {
  if (r.minute < 0) return "-";
  char hm[16];
  core::formatTime(r.minute / 60, r.minute % 60, app::settings().clock_24h, hm, sizeof(hm));
  return fmtStr("%s%s%s, %s, %s", r.label, r.type[0] ? " " : "", r.type, what.c_str(), hm);
}

void pageDayLog() {
  lv_obj_t* date = infoRow("Today", "");
  lv_obj_t* count = infoRow("Aircraft", "");
  lv_obj_t* busiest = infoRow("Busiest", "");
  lv_obj_t* closest = infoRow("Closest pass", "");
  lv_obj_t* highest = infoRow("Highest", "");
  lv_obj_t* fastest = infoRow("Fastest", "");
  lv_obj_t* special = infoRow("Military / emergencies", "");
  lv_obj_t* favs = infoRow("Favorites seen", "");
  for (lv_obj_t* l : {closest, highest, fastest, favs}) lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  auto& st = app::settings();
  sliderRow("Count aircraft within", 1, 100, static_cast<int>(app::kmToUnits(st.log_radius_km) + 0.5),
            [](int v) { app::settings().log_radius_km = static_cast<float>(app::unitsToKm(v)); },
            [](int v) { return fmtStr("%d %s", v, app::unitSuffix()); });

  section("EARLIER DAYS");
  const std::vector<core::daylog::Day> history = app::dayLogHistory(7);
  if (history.empty()) {
    label(s.list,
          app::maps::sdStatus().state == app::maps::SdState::Mounted ? "Nothing saved yet."
                                                                      : "With an SD card, every day is kept.",
          font(FontSize::S), pal().text_dim);
  }
  for (const core::daylog::Day& d : history) {
    char dist[24];
    app::formatDistance(d.closest.value, dist, sizeof(dist));
    lv_obj_t* l = label(s.list, core::daylog::summary(d, dist).c_str(), font(FontSize::M), pal().text);
    lv_obj_set_width(l, lv_pct(95));
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_pad_left(l, sc(16), 0);
  }
  lv_obj_t* note = label(s.list,
                         "Airborne aircraft within the distance above, counted while the radar shows the area "
                         "around home. Each day is saved on the SD card as log/YYYY-MM-DD.txt.",
                         font(FontSize::S), pal().text_dim);
  lv_obj_set_width(note, lv_pct(90));
  lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_pad_left(note, sc(16), 0);

  s.tick = [date, count, busiest, closest, highest, fastest, special, favs] {
    const core::daylog::Day d = app::dayLogToday();
    if (d.date == 0) {
      lv_label_set_text(date, "Waiting for the time (needs internet)");
      return;
    }
    static const char* const kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    lv_label_set_text(date, fmtStr("%s %d, %d", kMonths[(d.date / 100 % 100 + 11) % 12], d.date % 100, d.date / 10000).c_str());
    lv_label_set_text(count, fmtStr("%d different", d.count()).c_str());
    if (d.peak_minute >= 0) {
      char hm[16];
      core::formatTime(d.peak_minute / 60, d.peak_minute % 60, app::settings().clock_24h, hm, sizeof(hm));
      lv_label_set_text(busiest, fmtStr("%d at once, %s", d.peak, hm).c_str());
    } else {
      lv_label_set_text(busiest, "-");
    }
    char dist[24];
    app::formatDistance(d.closest.value, dist, sizeof(dist));
    lv_label_set_text(closest, recordText(d.closest, fmtStr("%s at %d ft", dist, static_cast<int>(d.closest.alt_ft))).c_str());
    lv_label_set_text(highest, recordText(d.highest, fmtStr("%d ft", static_cast<int>(d.highest.value))).c_str());
    lv_label_set_text(fastest, recordText(d.fastest, fmtStr("%d kt", static_cast<int>(d.fastest.value))).c_str());
    lv_label_set_text(special, fmtStr("%d / %d", static_cast<int>(d.military.size()), static_cast<int>(d.emergencies.size())).c_str());
    std::string f;
    for (const std::string& x : d.favorites) f += (f.empty() ? "" : ", ") + x;
    lv_label_set_text(favs, f.empty() ? "-" : f.c_str());
  };
}

/** One line for the update state (Updates page, and its row on the main page). */
std::string updateSummary() {
  const app::UpdateStatus u = app::updateStatus();
  switch (u.state) {
    case app::UpdateState::Idle: return app::settings().auto_update_check ? "Not checked yet" : "Not checked";
    case app::UpdateState::Checking: return "Checking...";
    case app::UpdateState::UpToDate: return "Up to date";
    case app::UpdateState::Available: return fmtStr("%s available", u.latest);
    case app::UpdateState::NoImage: return fmtStr("%s (flash over USB)", u.latest);
    case app::UpdateState::Failed: return u.error;
    case app::UpdateState::Installing: return "Installing...";
    case app::UpdateState::Installed: return "Installed - restarting";
  }
  return "";
}

void pageUpdates() {
  infoRow("Installed version", plat::firmwareVersion());
  lv_obj_t* latest = infoRow("Latest version", "");
  lv_label_set_long_mode(latest, LV_LABEL_LONG_WRAP);
  actionRow(LV_SYMBOL_REFRESH, "Check now", [] { app::checkForUpdate(); }, pal().accent);
  lv_obj_t* install = actionRow(LV_SYMBOL_DOWNLOAD, "Install update", [] { confirmInstallUpdate(); }, pal().accent);
  lv_obj_add_flag(install, LV_OBJ_FLAG_HIDDEN);

  section("AUTOMATIC CHECKS");
  switchRow("Check for updates automatically", &app::settings().auto_update_check);
  lv_obj_t* note = label(s.list,
                         "A minute after it starts and then every hour, the radar looks for a new version on "
                         "GitHub. Nothing is installed without asking: the update button on the radar lights up, "
                         "and you tap it to install. With this off, tap that button (or Check now) to look.",
                         font(FontSize::S), pal().text_dim);
  lv_obj_set_width(note, lv_pct(90));
  lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_pad_left(note, sc(16), 0);

  s.tick = [latest, install] {
    const app::UpdateStatus u = app::updateStatus();
    std::string text = updateSummary();
    if (u.state == app::UpdateState::NoImage) {
      text = fmtStr("%s - no update file for this board; flash it over USB", u.latest);
    }
    lv_label_set_text(latest, text.c_str());
    if (u.state == app::UpdateState::Available) {
      lv_label_set_text(lv_obj_get_child(install, 1), fmtStr("Install %s", u.latest).c_str());
      lv_obj_remove_flag(install, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(install, LV_OBJ_FLAG_HIDDEN);
    }
  };
}

void pageAbout() {
  lv_obj_t* fw = infoRow("Firmware", plat::firmwareVersion());
  (void)fw;
  infoRow("Display", fmtStr("%d x %d", static_cast<int>(screenW()), static_cast<int>(screenH())));
  lv_obj_t* wifi = infoRow("Wi-Fi", "");
  lv_obj_t* feed = infoRow("Feed", "");
  lv_obj_t* mem = infoRow("Free memory", "");

  section("DIAGNOSTICS");
  const plat::BootInfo boot = plat::bootInfo();
  lv_obj_t* up = infoRow("Running for", "");
  infoRow("Last restart", boot.reset_reason);
  infoRow("Starts", fmtStr("%u (%u unexpected)", static_cast<unsigned>(boot.boots),
                           static_cast<unsigned>(boot.unexpected_resets)));
  switchRow("On-screen stats", &app::settings().show_stats);
  lv_obj_t* note = label(s.list,
                         "Unexpected restarts are crashes, watchdog resets and power dips. On-screen stats show "
                         "frame rate, CPU, memory and more in the lower left corner.\n"
                         "Power-cycling three times in a row within 20 seconds of starting goes back to the "
                         "previously installed version.",
                         font(FontSize::S), pal().text_dim);
  lv_obj_set_width(note, lv_pct(90));
  lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_pad_left(note, sc(16), 0);

  section("MAINTENANCE");
  actionRow(LV_SYMBOL_REFRESH, "Restart", [] {
    app::settingsFlush();
    plat::restart();
  }, pal().text);
  actionRow(LV_SYMBOL_TRASH, "Factory reset", [] {
    confirm("Factory reset?",
            "Erases Wi-Fi, location and all settings (also settings.txt on the SD card), then restarts. "
            "favorites.txt on the card is kept.",
            "Erase", [] {
      app::factoryReset();
      plat::restart();
    });
  }, pal().danger);
  label(s.list, "Plane Radar P4 - aircraft data from community\nADS-B feeds; runways from OurAirports.",
        font(FontSize::S), pal().text_dim);

  s.tick = [wifi, feed, mem, up] {
    const unsigned secs = static_cast<unsigned>(plat::millis() / 1000);
    lv_label_set_text(up, secs >= 86400 ? fmtStr("%ud %uh %um", secs / 86400, secs / 3600 % 24, secs / 60 % 60).c_str()
                                        : fmtStr("%uh %02um %02us", secs / 3600, secs / 60 % 60, secs % 60).c_str());
    const plat::WifiStatus ws = plat::wifiStatus();
    lv_label_set_text(wifi, ws.state == plat::WifiState::Connected
                                ? fmtStr("%s, %d dBm", ws.ip, ws.rssi).c_str()
                                : "offline");
    const app::FeedStatus fs = app::feedStatus();
    if (fs.ever_ok) {
      const unsigned age = (plat::millis() - fs.last_ok_ms) / 1000;
      lv_label_set_text(feed, fmtStr("%d aircraft, %us ago (%s)", fs.last_count, age,
                                     core::adsb::sourceName(fs.source)).c_str());
    } else {
      lv_label_set_text(feed, fs.error[0] ? fs.error : "waiting");
    }
    lv_label_set_text(mem, fmtStr("%u KB RAM, %u KB PSRAM", static_cast<unsigned>(plat::freeInternalHeap() / 1024),
                                  static_cast<unsigned>(plat::freePsram() / 1024))
                               .c_str());
  };
}

void pagePhone() {
  auto& st = app::settings();
  const Palette& p = pal();
  // QR code on a white card (phones need the light border around it).
  lv_obj_t* box = lv_obj_create(s.list);
  lv_obj_remove_style_all(box);
  lv_obj_set_size(box, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(box, sc(10), 0);
  const std::string url = app::web::address();
  if (!st.web_enabled) {
    label(box, "The phone page is off.", font(FontSize::M), p.text_dim);
  } else if (url.empty()) {
    label(box, "Connect to Wi-Fi first.", font(FontSize::M), p.text_dim);
  } else {
    lv_obj_t* white = lv_obj_create(box);
    lv_obj_remove_style_all(white);
    lv_obj_set_size(white, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(white, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(white, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(white, sc(14), 0);
    lv_obj_set_style_pad_all(white, sc(16), 0);
    lv_obj_t* qr = lv_qrcode_create(white);
    lv_qrcode_set_size(qr, sc(250));
    lv_qrcode_set_dark_color(qr, lv_color_black());
    lv_qrcode_set_light_color(qr, lv_color_white());
    lv_qrcode_update(qr, url.data(), static_cast<uint32_t>(url.size()));
    lv_obj_t* hint = label(box, "Scan with your phone's camera", font(FontSize::M), p.text);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
  }
  if (st.web_enabled) {
    infoRow("Or open", url.empty() ? std::string("-") : url);
  }
  lv_obj_t* server = nullptr;
  lv_obj_t* visits = nullptr;
  if (st.web_enabled) {
    server = infoRow("Server", "");
    visits = infoRow("Phone visits", "");
    for (lv_obj_t* l : {server, visits}) lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    app::web::startSelfTest();
  }
  switchRow("Phone settings page", &st.web_enabled, [] { lv_async_call([](void*) { showPage(Page::Phone); }, nullptr); });
  lv_obj_t* note = label(s.list,
                         "Change settings, favorites and Wi-Fi networks from a phone or computer on the same "
                         "network, see the daily log and screenshots, and install a firmware file. Anyone on your "
                         "network can open it; it isn't reachable from the internet. Turn it off below when "
                         "you don't need it.",
                         font(FontSize::S), p.text_dim);
  lv_obj_set_width(note, lv_pct(90));
  lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_pad_left(note, sc(16), 0);
  lv_obj_t* tips = label(s.list,
                         "If the page doesn't open on the phone: the phone must be on the same Wi-Fi as the "
                         "radar, not mobile data or a guest network (guest networks keep devices apart). "
                         "\"Phone visits\" above counts every time a phone reached the radar.",
                         font(FontSize::S), p.text_dim);
  lv_obj_set_width(tips, lv_pct(90));
  lv_label_set_long_mode(tips, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_pad_left(tips, sc(16), 0);
  // The address appears once Wi-Fi connects.
  const bool had_url = !url.empty();
  s.tick = [had_url, server, visits] {
    if (!app::web::address().empty() != had_url) showPage(Page::Phone);
    if (server == nullptr) return;
    const plat::WebStats ws = plat::webStats();
    const int test = app::web::selfTest();
    if (!ws.running) lv_label_set_text(server, ws.error[0] ? ws.error : "Not running");
    else if (test == 0) lv_label_set_text(server, "Running, checking...");
    else lv_label_set_text(server, test > 0 ? "Running and answering" : "Running, but not answering");
    if (ws.requests == 0) {
      lv_label_set_text(visits, "None yet");
    } else {
      const unsigned ago = (plat::millis() - ws.last_ms) / 1000;
      lv_label_set_text(visits, fmtStr("%u, last from %s, %s ago", static_cast<unsigned>(ws.requests), ws.last_client,
                                       ago < 120 ? fmtStr("%u s", ago).c_str() : fmtStr("%u min", ago / 60).c_str())
                                    .c_str());
    }
  };
}

void pageWatchlist() {
  auto& st = app::settings();
  const Palette& p = pal();
  label(s.list, "Announce these aircraft when they come near home.", font(FontSize::S), p.text_dim);
  switchRow("Watchlist alerts", &st.watch_enabled);
  section("ANNOUNCE");
  switchRow("Military aircraft", &st.watch_military);
  switchRow("Helicopters", &st.watch_rotor);
  switchRow("Fast jets", &st.watch_fast);
  const std::string types = st.watch_types[0] ? st.watch_types : "none";
  actionRow(LV_SYMBOL_KEYBOARD, ("Types: " + types).c_str(), [] {
    textInput("Types, e.g. A388, B52, C17, F35", app::settings().watch_types, InputKind::Text,
              [](const std::string& v) {
                auto& w = app::settings();
                std::string up;
                for (char c : v) up += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                std::snprintf(w.watch_types, sizeof(w.watch_types), "%s", up.c_str());
                changed();
                showPage(Page::Watchlist);
              });
  }, p.text);
  const std::string calls = st.watch_callsigns[0] ? st.watch_callsigns : "none";
  actionRow(LV_SYMBOL_KEYBOARD, ("Callsigns: " + calls).c_str(), [] {
    textInput("Callsigns or tail numbers, e.g. RCH*, N123AB", app::settings().watch_callsigns, InputKind::Text,
              [](const std::string& v) {
                auto& w = app::settings();
                std::string up;
                for (char c : v) up += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                std::snprintf(w.watch_callsigns, sizeof(w.watch_callsigns), "%s", up.c_str());
                changed();
                showPage(Page::Watchlist);
              });
  }, p.text);
  label(s.list, "* matches a prefix: A38* = any A380, RCH* = US Air Force cargo flights.", font(FontSize::S),
        p.text_dim);
  section("WHEN");
  sliderRow("Within", 1, 150, static_cast<int>(app::kmToUnits(st.watch_radius_km) + 0.5),
            [](int v) { app::settings().watch_radius_km = static_cast<float>(app::unitsToKm(v)); },
            [](int v) { return fmtStr("%d %s of home", v, app::unitSuffix()); });
  switchRow("Play a sound", &st.sound_watch);
  actionRow(LV_SYMBOL_PLAY, "Test the sound", [] { plat::playSound(core::Sound::Watch); }, p.text);
  lv_obj_t* note = label(s.list,
                         "Each aircraft is announced once per visit, with a message on the screen (and the "
                         "sound). Watched aircraft are always shown, whatever the filters.",
                         font(FontSize::S), p.text_dim);
  lv_obj_set_width(note, lv_pct(90));
  lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_pad_left(note, sc(16), 0);
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
    case Page::DayLog: return "Daily log";
    case Page::Updates: return "Updates";
    case Page::About: return "About";
    case Page::CustomTheme: return "Custom colors";
    case Page::Clock: return "Clock & alarms";
    case Page::TimeZone: return "Time zone";
    case Page::AlarmEdit: return "Alarm";
    case Page::SdCard: return "SD card";
    case Page::Phone: return "Phone settings page";
    case Page::Watchlist: return "Watchlist alerts";
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
    case Page::DayLog: pageDayLog(); break;
    case Page::Updates: pageUpdates(); break;
    case Page::About: pageAbout(); break;
    case Page::CustomTheme: pageCustomTheme(); break;
    case Page::Clock: pageClock(); break;
    case Page::TimeZone: pageTimeZone(); break;
    case Page::AlarmEdit: pageAlarmEdit(); break;
    case Page::SdCard: pageSdCard(); break;
    case Page::Phone: pagePhone(); break;
    case Page::Watchlist: pageWatchlist(); break;
    default: pageMain(); break;
  }
  if (s.tick) s.tick();
}

void onTimer(lv_timer_t*) {
  if (s.tick) s.tick();
}

}  // namespace

Page currentPage() { return s.page; }
int32_t scrollY() { return s.list ? lv_obj_get_scroll_y(s.list) : 0; }

lv_obj_t* create(Page page, int32_t scroll_y) {
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
    else if (s.page == Page::CustomTheme) showPage(Page::Display);
    else if (s.page == Page::TimeZone || s.page == Page::AlarmEdit) showPage(Page::Clock);
    else showPage(Page::Main);
  });
  lv_obj_align(back, LV_ALIGN_CENTER, -sc(196), -sc(286));
  // X throws away this visit's changes; < keeps them.
  lv_obj_t* close = iconButton(s.screen, LV_SYMBOL_CLOSE, sc(72), [] { closeSettings(true); });
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
  if (scroll_y > 0) {
    lv_obj_update_layout(s.list);
    lv_obj_scroll_to_y(s.list, scroll_y, LV_ANIM_OFF);
  }
  return s.screen;
}

void destroy() {
  if (s.timer) lv_timer_delete(s.timer);
  s.timer = nullptr;
  s.tick = nullptr;
}

}  // namespace ui::settings_ui
