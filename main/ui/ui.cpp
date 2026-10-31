#include "ui/ui.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "app/app.h"
#include "app/maps.h"
#include "core/clock.h"
#include "platform/platform.h"
#include "ui/display_rotation.h"
#include "ui/hud.h"
#include "ui/radar_view.h"
#include "ui/settings_ui.h"
#include "ui/sprites.h"
#include "ui/theme.h"
#include "ui/widgets.h"
// lv_display_t internals (the dirty areas), for the on-screen stats.
#if __has_include("lvgl_private.h")
#include "lvgl_private.h"
#else
#include "src/lvgl_private.h"
#endif

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

// On-screen stats (Settings > About > On-screen stats).
lv_obj_t* s_stats = nullptr;
uint32_t s_frames = 0;           // frames drawn since the last stats update
uint64_t s_draw_us = 0, s_draw_max_us = 0;
uint64_t s_refr_start_us = 0;
bool s_rendered = false;
uint64_t s_redrawn_px = 0;      // pixels redrawn since the last stats update
uint32_t s_stats_gap_ms = 0;     // longest pause of the UI timer since the last update

// Firmware update in progress: a full-screen box with a progress bar.
lv_obj_t* s_update_box = nullptr;
lv_obj_t* s_update_bar = nullptr;
lv_obj_t* s_update_text = nullptr;
app::UpdateState s_update_shown = app::UpdateState::Idle;
uint32_t s_restart_at_ms = 0;

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

void updateUpdateBox() {
  const app::UpdateStatus u = app::updateStatus();
  const bool busy = u.state == app::UpdateState::Installing || u.state == app::UpdateState::Installed;
  if (!busy) {
    if (s_update_box) {
      lv_obj_delete(s_update_box);
      s_update_box = s_update_bar = s_update_text = nullptr;
      if (u.state == app::UpdateState::Failed && u.install_failed) {
        toast((std::string(LV_SYMBOL_WARNING "  Update failed: ") + u.error).c_str(), 6000);
      }
    }
    s_update_shown = u.state;
    return;
  }
  const Palette& p = pal();
  if (s_update_box == nullptr) {
    s_update_box = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_update_box);
    lv_obj_set_size(s_update_box, screenW(), screenH());
    lv_obj_set_style_bg_color(s_update_box, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_update_box, 230, 0);
    lv_obj_add_flag(s_update_box, LV_OBJ_FLAG_CLICKABLE);  // nothing else can be used meanwhile
    lv_obj_set_flex_flow(s_update_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_update_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_update_box, sc(22), 0);
    label(s_update_box, (std::string(LV_SYMBOL_DOWNLOAD "  Updating to ") + u.latest).c_str(), font(FontSize::L),
          p.accent);
    s_update_bar = lv_bar_create(s_update_box);
    lv_obj_set_size(s_update_bar, sc(440), sc(22));
    lv_bar_set_range(s_update_bar, 0, 1000);
    lv_obj_set_style_bg_color(s_update_bar, p.panel_border, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_update_bar, p.accent, LV_PART_INDICATOR);
    s_update_text = label(s_update_box, "", font(FontSize::M), p.text);
    label(s_update_box, "Keep the power on", font(FontSize::S), p.text_dim);
  }
  char text[64];
  if (u.state == app::UpdateState::Installed) {
    lv_bar_set_value(s_update_bar, 1000, LV_ANIM_OFF);
    std::snprintf(text, sizeof(text), "Installed - restarting...");
    if (s_update_shown != app::UpdateState::Installed) {
      app::settingsFlush();  // the SD card copy is written meanwhile by the network task
      s_restart_at_ms = plat::millis() + 2500;
    } else if (s_restart_at_ms != 0 && static_cast<int32_t>(plat::millis() - s_restart_at_ms) >= 0) {
      s_restart_at_ms = 0;
      plat::restart();
    }
  } else if (u.total > 0) {
    lv_bar_set_value(s_update_bar, static_cast<int32_t>(u.done * 1000 / u.total), LV_ANIM_OFF);
    std::snprintf(text, sizeof(text), "%.1f of %.1f MB", u.done / 1e6, u.total / 1e6);
  } else {
    std::snprintf(text, sizeof(text), "Connecting...");
  }
  lv_label_set_text(s_update_text, text);
  s_update_shown = u.state;
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
  const plat::Health hl = plat::sampleHealth(0);
  std::printf("health: RAM %u KB free (lowest %u, largest block %u), PSRAM %u KB; CPU core0 %d%%, core1 %d%%: %s; "
              "UI longest pause %u ms (%s)\n",
              static_cast<unsigned>(hl.ram_free / 1024), static_cast<unsigned>(hl.ram_min / 1024),
              static_cast<unsigned>(hl.ram_largest / 1024), static_cast<unsigned>(hl.psram_free / 1024), hl.cpu[0],
              hl.cpu[1], hl.top.c_str(), static_cast<unsigned>(s_worst_gap), s_settings_screen ? "menu" : "radar");
  s_report_ms = now;
  s_worst_gap = 0;
}

/**
 * Short messages as the Wi-Fi state changes: connecting, connected, lost,
 * and why it failed. The driver retries every few seconds, so each message
 * is shown once per network until the state really changes.
 */
void announceWifi() {
  static plat::WifiState s_last = plat::WifiState::Idle;
  static std::string s_connecting_shown, s_failure_shown, s_connected_ssid;
  const plat::WifiStatus ws = plat::wifiStatus();
  const std::string ssid = ws.ssid;
  char text[96];
  if (ws.state == plat::WifiState::Connected) {
    if (s_last != plat::WifiState::Connected || ssid != s_connected_ssid) {
      std::snprintf(text, sizeof(text), LV_SYMBOL_WIFI "  Connected to %s", ws.ssid);
      toast(text, 2500);
    }
    s_connected_ssid = ssid;
    s_connecting_shown.clear();
    s_failure_shown.clear();
  } else {
    if (s_last == plat::WifiState::Connected) {
      std::snprintf(text, sizeof(text), LV_SYMBOL_WARNING "  Lost Wi-Fi (%s) - reconnecting", s_connected_ssid.c_str());
      toast(text, 3000);
      s_connecting_shown = s_connected_ssid;  // that says it all for now
      s_failure_shown = s_connected_ssid + "|" + ws.reason;
    } else if (ws.state == plat::WifiState::Connecting && !ssid.empty() && ssid != s_connecting_shown) {
      std::snprintf(text, sizeof(text), LV_SYMBOL_WIFI "  Connecting to %s...", ws.ssid);
      toast(text, 2500);
      s_connecting_shown = ssid;
    } else if (ws.state == plat::WifiState::Failed && ws.reason[0] != '\0') {
      const std::string key = ssid + "|" + ws.reason;
      if (key != s_failure_shown) {
        std::snprintf(text, sizeof(text), LV_SYMBOL_WARNING "  Can't connect to %s: %s", ws.ssid, ws.reason);
        toast(text, 4000);
        s_failure_shown = key;
      }
    }
    s_connected_ssid.clear();
  }
  s_last = ws.state;
}

void onRefrStart(lv_event_t* e) {
  s_refr_start_us = plat::micros();
  s_rendered = false;
  // How much of the screen this frame redraws (LVGL has merged the dirty areas by now).
  auto* disp = static_cast<lv_display_t*>(lv_event_get_target(e));
  for (uint32_t i = 0; i < disp->inv_p && i < LV_INV_BUF_SIZE; ++i) {
    if (!disp->inv_area_joined[i]) s_redrawn_px += static_cast<uint64_t>(lv_area_get_size(&disp->inv_areas[i]));
  }
}
void onRenderReady(lv_event_t*) {
  s_rendered = true;
  ++s_frames;
}
void onRefrReady(lv_event_t*) {
  if (!s_rendered) return;
  const uint64_t us = plat::micros() - s_refr_start_us;
  s_draw_us += us;
  s_draw_max_us = std::max(s_draw_max_us, us);
}

/** Small live numbers in the lower left, once a second: for spotting slowdowns as they happen. */
void updateStats() {
  static uint32_t s_last_ms = 0, s_prev_tick_ms = 0;
  const uint32_t now = plat::millis();
  if (s_prev_tick_ms != 0) s_stats_gap_ms = std::max(s_stats_gap_ms, now - s_prev_tick_ms);
  s_prev_tick_ms = now;
  const bool on = app::settings().show_stats;
  if (!on) {
    if (s_stats) lv_obj_delete(s_stats);
    s_stats = nullptr;
    return;
  }
  if (s_stats == nullptr) {
    s_stats = lv_label_create(lv_layer_sys());  // above dialogs too
    lv_obj_set_style_text_font(s_stats, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_stats, lv_color_white(), 0);
    lv_obj_set_style_bg_color(s_stats, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_stats, 160, 0);
    lv_obj_set_style_pad_hor(s_stats, sc(8), 0);
    lv_obj_set_style_pad_ver(s_stats, sc(4), 0);
    lv_obj_set_style_radius(s_stats, sc(8), 0);
    lv_obj_remove_flag(s_stats, LV_OBJ_FLAG_CLICKABLE);
    // Lower left, inside the round screen and clear of the buttons on its edge.
    lv_obj_align(s_stats, LV_ALIGN_CENTER, -static_cast<int32_t>(screenW() * 0.13f),
                 static_cast<int32_t>(screenH() * 0.17f));
    lv_label_set_text(s_stats, "stats...");
    s_last_ms = now;
    s_frames = 0;
    s_draw_us = s_draw_max_us = 0;
    s_redrawn_px = 0;
    s_stats_gap_ms = 0;
    plat::sampleHealth(1);  // CPU baseline
    return;
  }
  if (now - s_last_ms < 1000) return;
  const float secs = (now - s_last_ms) / 1000.0f;
  const plat::Health h = plat::sampleHealth(1);
  const plat::WifiStatus ws = plat::wifiStatus();
  const app::FeedStatus fs = app::feedStatus();
  const uint32_t up = now / 1000;
  // Where the frame time goes, per frame over the last second: LVGL drawing,
  // handing the picture to the panel (rotation + copy), and the radar's own
  // drawing (its overlay, and rebuilding the background layer).
  static FlushStats s_flush0;
  static RadarView::Stats s_radar0;
  const FlushStats fl = flushStats();
  const RadarView::Stats& rt = RadarView::get().totals();
  const double frames = s_frames ? static_cast<double>(s_frames) : 1.0;
  const double flush_ms = (fl.flush_us - s_flush0.flush_us) / 1000.0 / frames;
  const double rotate_ms = (fl.rotate_us - s_flush0.rotate_us) / 1000.0 / frames;
  const double frame_ms = s_frames ? s_draw_us / 1000.0 / frames : 0.0;
  const uint32_t radar_frames = rt.frames - s_radar0.frames;
  const double overlay_ms = radar_frames ? (rt.overlay_us - s_radar0.overlay_us) / 1000.0 / radar_frames : 0.0;
  const uint32_t statics = rt.static_renders - s_radar0.static_renders;
  const double static_ms = (rt.static_us - s_radar0.static_us) / 1000.0;
  const double screen_px = static_cast<double>(screenW()) * screenH();
  const int redrawn = s_frames ? static_cast<int>(100.0 * s_redrawn_px / (screen_px * frames) + 0.5) : 0;
  s_flush0 = fl;
  s_radar0 = rt;
  char text[384];
  std::snprintf(text, sizeof(text),
                "%.0f fps, frame %.1f ms (max %.0f), %d%% redrawn\n"
                "draw %.1f + panel %.1f ms (rotate %.1f)\n"
                "radar %.1f ms, background %.0f ms (%u)\n"
                "CPU %d%% net, %d%% UI\n"
                "RAM %u K (low %u K), PSRAM %.1f M\n"
                "%d aircraft (decode %.0f ms), %s\n"
                "up %u:%02u:%02u, UI pause %u ms",
                s_frames / secs, frame_ms, s_draw_max_us / 1000.0, redrawn, std::max(0.0, frame_ms - flush_ms), flush_ms,
                rotate_ms, overlay_ms, static_ms, static_cast<unsigned>(statics), h.cpu[0], h.cpu[1],
                static_cast<unsigned>(h.ram_free / 1024), static_cast<unsigned>(h.ram_min / 1024),
                h.psram_free / 1048576.0, fs.last_count, fs.parse_us / 1000.0,
                ws.state == plat::WifiState::Connected ? (std::to_string(ws.rssi) + " dBm").c_str() : "offline",
                static_cast<unsigned>(up / 3600), static_cast<unsigned>(up / 60 % 60), static_cast<unsigned>(up % 60),
                static_cast<unsigned>(s_stats_gap_ms));
  lv_label_set_text(s_stats, text);
  lv_obj_move_foreground(s_stats);
  s_last_ms = now;
  s_frames = 0;
  s_draw_us = s_draw_max_us = 0;
  s_redrawn_px = 0;
  s_stats_gap_ms = 0;
}

void onAppTimer(lv_timer_t*) {
  logHealth();
  updateStats();
  announceWifi();
  updateSleepLayer();
  updateAlarmDialog();
  updateUpdateBox();
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
  lv_display_add_event_cb(disp, onRefrStart, LV_EVENT_REFR_START, nullptr);
  lv_display_add_event_cb(disp, onRenderReady, LV_EVENT_RENDER_READY, nullptr);
  lv_display_add_event_cb(disp, onRefrReady, LV_EVENT_REFR_READY, nullptr);

  // First boot: go straight to Wi-Fi setup.
  if (app::settings().wifi_ssid[0] == '\0') openSettings(Page::Wifi);

  app::onSettingsLoaded(onSettingsLoaded);
  app::onScreenshotRequest([] { return takeScreenshot(); });
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

bool takeScreenshot() {
  if (app::maps::sdStatus().state != app::maps::SdState::Mounted) {
    toast(LV_SYMBOL_SD_CARD "  Screenshots need an SD card", 2500);
    return false;
  }
  const int32_t w = screenW(), h = screenH();
  const uint32_t stride = lv_draw_buf_width_to_stride(w, LV_COLOR_FORMAT_RGB888);
  std::vector<uint8_t> px(static_cast<size_t>(stride) * h);
  lv_draw_buf_t buf;
  lv_draw_buf_init(&buf, w, h, LV_COLOR_FORMAT_RGB888, stride, px.data(), px.size());
  const bool radar = lv_screen_active() == s_radar_screen;
  const bool was_hidden = radar && hud::hideForSnapshot(true);
  const lv_result_t res = lv_snapshot_take_to_draw_buf(lv_screen_active(), LV_COLOR_FORMAT_RGB888, &buf);
  if (radar) hud::hideForSnapshot(was_hidden);
  if (res != LV_RESULT_OK || !app::saveScreenshot(std::move(px), w, h, static_cast<int>(stride))) {
    toast(LV_SYMBOL_WARNING "  Couldn't take a screenshot (still saving the last one?)", 2500);
    return false;
  }
  app::sound(core::Sound::Click);
  toast(LV_SYMBOL_IMAGE "  Saving screenshot...", 1500);
  return true;
}

void confirmInstallUpdate() {
  const app::UpdateStatus u = app::updateStatus();
  if (u.state != app::UpdateState::Available) return;
  char title[48], body[320];
  std::snprintf(title, sizeof(title), "Install %s?", u.latest);
  std::snprintf(body, sizeof(body),
                "You have %s. The update downloads about %u MB from GitHub and restarts; it takes a few "
                "minutes, so keep the power on. Settings and favorites are kept, and if the new version "
                "doesn't start, the radar goes back to this one.",
                plat::firmwareVersion(), static_cast<unsigned>((u.total + 500000) / 1000000));
  confirm(title, body, "Install", [] { app::installUpdate(); });
}

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
    std::memcpy(s_snapshot.wifi_saved, st.wifi_saved, sizeof(st.wifi_saved));
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
