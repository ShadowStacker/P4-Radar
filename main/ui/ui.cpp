#include "ui/ui.h"

#include <cstring>
#include <vector>

#include "app/app.h"
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

void onAppTimer(lv_timer_t*) {
  // Any touch on any screen keeps the backlight up.
  if (lv_display_get_inactive_time(s_disp) < 400) app::userActivity();
  app::tick(plat::millis());
}

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

}  // namespace

void openSettings(Page page) {
  if (s_settings_screen != nullptr) return;
  s_snapshot = app::settings();
  s_snapshot_gen = app::settingsGeneration();
  s_fav_snapshot = app::favorites();
  RadarView::get().setPaused(true);
  s_settings_screen = settings_ui::create(page);
  lv_screen_load_anim(s_settings_screen, LV_SCR_LOAD_ANIM_FADE_IN, 180, 0, false);
}

void closeSettings(bool discard) {
  if (s_settings_screen == nullptr) return;
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
