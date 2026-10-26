#include "ui/ui.h"

#include "app/app.h"
#include "platform/platform.h"
#include "ui/hud.h"
#include "ui/radar_view.h"
#include "ui/settings_ui.h"
#include "ui/sprites.h"
#include "ui/theme.h"

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
  setTheme(app::settings().theme);
  applyLvglTheme(disp);
  sprites::init();

  buildRadarScreen();
  lv_screen_load(s_radar_screen);
  s_app_timer = lv_timer_create(onAppTimer, 250, nullptr);

  // First boot: go straight to Wi-Fi setup.
  if (app::settings().wifi_ssid[0] == '\0') openSettings(Page::Wifi);
}

void openSettings(Page page) {
  if (s_settings_screen != nullptr) return;
  RadarView::get().setPaused(true);
  s_settings_screen = settings_ui::create(page);
  lv_screen_load_anim(s_settings_screen, LV_SCR_LOAD_ANIM_FADE_IN, 180, 0, false);
}

void closeSettings() {
  if (s_settings_screen == nullptr) return;
  settings_ui::destroy();
  RadarView::get().setPaused(false);
  lv_screen_load_anim(s_radar_screen, LV_SCR_LOAD_ANIM_FADE_IN, 180, 0, true);
  s_settings_screen = nullptr;  // deleted by the load animation (auto_del)
}

void rebuild() {
  const bool in_settings = s_settings_screen != nullptr;
  setTheme(app::settings().theme);
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
    s_settings_screen = settings_ui::create(Page::Display);
    lv_screen_load(s_settings_screen);
  } else {
    lv_screen_load(s_radar_screen);
  }
  if (old_settings) lv_obj_delete(old_settings);
  lv_obj_delete(old_radar);
}

}  // namespace ui
