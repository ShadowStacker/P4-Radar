#include "ui/ui.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "app/app.h"
#include "core/clock.h"
#include "platform/platform.h"
#include "ui/hud.h"
#include "ui/radar_view.h"
#include "ui/settings_ui.h"
#include "ui/sprites.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace ui {

namespace {

lv_display_t* s_disp = nullptr;
lv_obj_t* s_radar_screen = nullptr;
lv_obj_t* s_settings_screen = nullptr;
lv_timer_t* s_app_timer = nullptr;

void buildRadarScreen() {
  s_radar_screen = lv_obj_create(nullptr);
  lv_obj_remove_flag(s_radar_screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(s_radar_screen, pal().bg, 0);
  RadarView::get().create(s_radar_screen);
  hud::create(s_radar_screen);
}

// Alarm dialog on the top layer, so it shows over the radar or settings.
lv_obj_t* s_alarm_box = nullptr;
lv_obj_t* s_alarm_time = nullptr;

std::string clockText() {
  const int64_t now = plat::unixTimeMs() / 1000;
  const std::tm t = core::localTime(now);
  char hm[16];
  core::formatTime(t.tm_hour, t.tm_min, app::settings().clock_24h, hm, sizeof(hm));
  return hm;
}

void closeAlarmDialog() {
  if (s_alarm_box) lv_obj_delete(s_alarm_box);
  s_alarm_box = nullptr;
  s_alarm_time = nullptr;
}

void updateAlarmDialog() {
  const int ringing = app::alarmRinging();
  if (ringing < 0) {
    closeAlarmDialog();
    return;
  }
  if (s_alarm_box) {
    lv_label_set_text(s_alarm_time, clockText().c_str());
    return;
  }
  const Palette& p = pal();
  s_alarm_box = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(s_alarm_box);
  lv_obj_set_size(s_alarm_box, screenW(), screenH());
  lv_obj_set_style_bg_color(s_alarm_box, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(s_alarm_box, 215, 0);
  lv_obj_add_flag(s_alarm_box, LV_OBJ_FLAG_CLICKABLE);  // swallow taps
  lv_obj_set_flex_flow(s_alarm_box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(s_alarm_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(s_alarm_box, sc(18), 0);

  label(s_alarm_box, LV_SYMBOL_BELL "  Alarm", font(FontSize::L), p.accent);
  s_alarm_time = label(s_alarm_box, clockText().c_str(), &lv_font_montserrat_44, p.text);
  const core::Alarm& a = app::settings().alarms[ringing];
  char set[16];
  core::formatTime(a.hour, a.minute, app::settings().clock_24h, set, sizeof(set));
  label(s_alarm_box, (std::string("Set for ") + set).c_str(), font(FontSize::M), p.text_dim);

  lv_obj_t* row = lv_obj_create(s_alarm_box);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, sc(520), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_top(row, sc(20), 0);
  pillButton(row, "Snooze 9 min", [] {
    app::snoozeAlarm();
    closeAlarmDialog();
    toast(LV_SYMBOL_BELL "  Snoozed for 9 minutes", 2500);
  });
  pillButton(row, LV_SYMBOL_OK "  Stop", [] {
    app::stopAlarm();
    closeAlarmDialog();
  }, true);
}

// While asleep: a black, full-screen layer that takes the waking tap, so the
// tap can't also press whatever is under it.
lv_obj_t* s_sleep_layer = nullptr;

void onSleepPressed(lv_event_t*) {
  lv_indev_t* indev = lv_indev_active();
  if (indev) lv_indev_wait_release(indev);  // the rest of this touch does nothing
  app::setSleeping(false);
}

void updateSleepLayer() {
  const bool asleep = app::sleeping();
  if (asleep && s_sleep_layer == nullptr) {
    s_sleep_layer = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_sleep_layer);
    lv_obj_set_size(s_sleep_layer, screenW(), screenH());
    lv_obj_set_style_bg_color(s_sleep_layer, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_sleep_layer, LV_OPA_COVER, 0);
    lv_obj_add_flag(s_sleep_layer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_sleep_layer, onSleepPressed, LV_EVENT_PRESSED, nullptr);
    lv_obj_move_background(s_sleep_layer);  // an alarm dialog stays on top of it
  } else if (!asleep && s_sleep_layer != nullptr) {
    lv_obj_delete_async(s_sleep_layer);  // we may be inside its own event
    s_sleep_layer = nullptr;
  }
}

/**
 * Every 30 s, a "health:" line in the serial log: memory, CPU load and the
 * longest the UI went without running this 250 ms timer (a stall that long
 * is a frozen screen or missed touches).
 */
void logHealth() {
  static uint32_t s_last_ms = 0, s_report_ms = 0, s_worst_gap = 0;
  const uint32_t now = plat::millis();
  if (s_last_ms != 0) s_worst_gap = std::max(s_worst_gap, now - s_last_ms);
  s_last_ms = now;
  if (s_report_ms == 0) s_report_ms = now;
  if (now - s_report_ms < 30000) return;
  std::printf("health: %s; UI longest pause %u ms (%s)\n", plat::healthReport().c_str(),
              static_cast<unsigned>(s_worst_gap), s_settings_screen ? "menu" : "radar");
  s_report_ms = now;
  s_worst_gap = 0;
}

void onAppTimer(lv_timer_t*) {
  logHealth();
  updateSleepLayer();
  updateAlarmDialog();
  // Any touch on any screen keeps the backlight up.
  if (lv_display_get_inactive_time(s_disp) < 400) app::userActivity();
  app::tick(plat::millis());
}

void onSettingsLoaded(const core::Settings& before);

}  // namespace

void init(lv_display_t* disp) {
  s_disp = disp;
  setScreenSize(lv_display_get_horizontal_resolution(disp), lv_display_get_vertical_resolution(disp));
  setTheme(app::settings());
  applyLvglTheme(disp);
  sprites::init(app::settings().aircraft_scale);

  buildRadarScreen();
  lv_screen_load(s_radar_screen);
  s_app_timer = lv_timer_create(onAppTimer, 250, nullptr);

  // First boot: go straight to Wi-Fi setup.
  if (app::settings().wifi_ssid[0] == '\0') openSettings(Page::Wifi);

  app::onSettingsLoaded(onSettingsLoaded);
}

namespace {

// What settings looked like when they were opened, for X (discard).
core::Settings s_snapshot;
uint32_t s_snapshot_gen = 0;
std::vector<core::Favorite> s_fav_snapshot;

bool sameFavorites(const std::vector<core::Favorite>& a, const std::vector<core::Favorite>& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (std::strcmp(a[i].hex, b[i].hex) != 0) return false;
  }
  return true;
}

/** settings.txt on the SD card replaced the settings: redraw everything with them. */
void onSettingsLoaded(const core::Settings& before) {
  const core::Settings& st = app::settings();
  if (before.rotation != st.rotation) {
    plat::restart();  // the orientation is set up at boot (the new one is already saved)
    return;
  }
  sprites::init(st.aircraft_scale);
  if (s_settings_screen != nullptr) {
    // X shouldn't undo what came from the card.
    s_snapshot = st;
    s_snapshot_gen = app::settingsGeneration();
    if (before.wifi_ssid[0] == '\0' && st.wifi_ssid[0] != '\0') {
      // First-time setup is done: the card brought the network.
      lv_obj_t* old = s_settings_screen;
      settings_ui::destroy();
      s_settings_screen = nullptr;
      app::setMenuOpen(false);
      RadarView::get().setPaused(false);
      rebuild();
      lv_obj_delete(old);
      return;
    }
  }
  rebuild();
}

}  // namespace

bool settingsOpen() { return s_settings_screen != nullptr; }

void openSettings(Page page) {
  if (s_settings_screen != nullptr) return;
  s_snapshot = app::settings();
  s_snapshot_gen = app::settingsGeneration();
  s_fav_snapshot = app::favorites();
  RadarView::get().setPaused(true);
  app::setMenuOpen(true);
  s_settings_screen = settings_ui::create(page);
  lv_screen_load_anim(s_settings_screen, LV_SCR_LOAD_ANIM_FADE_IN, 180, 0, false);
}

void closeSettings(bool discard) {
  if (s_settings_screen == nullptr) return;
  app::setMenuOpen(false);
  const bool settings_changed = app::settingsGeneration() != s_snapshot_gen;
  const bool favs_changed = !sameFavorites(app::favorites(), s_fav_snapshot);
  if (discard && (settings_changed || favs_changed)) {
    // Undo everything except Wi-Fi, then re-theme with the old settings.
    core::Settings& st = app::settings();
    std::memcpy(s_snapshot.wifi_ssid, st.wifi_ssid, sizeof(st.wifi_ssid));
    std::memcpy(s_snapshot.wifi_pass, st.wifi_pass, sizeof(st.wifi_pass));
    const bool filters_changed =
        st.military_only != s_snapshot.military_only || st.show_ground != s_snapshot.show_ground ||
        st.min_alt_ft != s_snapshot.min_alt_ft || st.max_alt_ft != s_snapshot.max_alt_ft ||
        st.category_mask != s_snapshot.category_mask || st.min_speed_kt != s_snapshot.min_speed_kt ||
        st.max_speed_kt != s_snapshot.max_speed_kt || std::strcmp(st.type_filter, s_snapshot.type_filter) != 0;
    st = s_snapshot;
    if (favs_changed) app::setFavorites(s_fav_snapshot);
    app::settingsChanged();
    app::settingsFlush();
    if (filters_changed) {
      app::traffic().clear();  // aircraft the restored filters hide (or show) change
      app::requestRefresh();
    }
    sprites::init(st.aircraft_scale);
    lv_obj_t* old = s_settings_screen;
    settings_ui::destroy();
    s_settings_screen = nullptr;
    rebuild();  // new radar screen with the restored theme
    RadarView::get().setPaused(false);
    lv_obj_delete(old);
    toast(LV_SYMBOL_CLOSE "  Changes discarded");
    return;
  }
  settings_ui::destroy();
  RadarView::get().setPaused(false);
  lv_screen_load_anim(s_radar_screen, LV_SCR_LOAD_ANIM_FADE_IN, 180, 0, true);
  s_settings_screen = nullptr;  // deleted by the load animation (auto_del)
}

void rebuild() {
  const bool in_settings = s_settings_screen != nullptr;
  const Page page = in_settings ? settings_ui::currentPage() : Page::Display;
  const int32_t scroll = in_settings ? settings_ui::scrollY() : 0;
  setTheme(app::settings());
  applyLvglTheme(s_disp);

  lv_obj_t* old_radar = s_radar_screen;
  lv_obj_t* old_settings = s_settings_screen;
  if (old_settings) settings_ui::destroy();
  s_settings_screen = nullptr;

  hud::destroy();
  RadarView::get().destroy();
  buildRadarScreen();

  if (in_settings) {
    RadarView::get().setPaused(true);
    s_settings_screen = settings_ui::create(page, scroll);
    lv_screen_load(s_settings_screen);
  } else {
    lv_screen_load(s_radar_screen);
  }
  if (old_settings) lv_obj_delete(old_settings);
  lv_obj_delete(old_radar);
}

}  // namespace ui
