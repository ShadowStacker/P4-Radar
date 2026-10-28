#include "ui/hud.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "app/app.h"
#include "core/adsb.h"
#include "core/filter.h"
#include "platform/platform.h"
#include "ui/radar_view.h"
#include "ui/sprites.h"
#include "ui/theme.h"
#include "ui/ui.h"
#include "ui/widgets.h"

namespace ui::hud {

namespace {

struct Hud {
  lv_obj_t* root = nullptr;
  lv_obj_t* controls = nullptr;  // holds the round buttons so they can fade together
  bool controls_hidden = false;
  uint32_t hide_done_ms = 0;     // when the fade-out finishes (then the layer is hidden)
  lv_obj_t* status = nullptr;
  lv_obj_t* status_label = nullptr;
  bool status_problem = false;   // offline/errors: keep everything visible
  lv_obj_t* filter = nullptr;
  lv_obj_t* mute = nullptr;
  lv_obj_t* mute_label = nullptr;
  lv_obj_t* clock = nullptr;
  lv_obj_t* sleep = nullptr;
  lv_obj_t* alt_key = nullptr;    // altitude color scale under the status pill
  lv_obj_t* alarm_box = nullptr;  // "alarm ringing" dialog, while shown
  lv_obj_t* gear = nullptr;
  lv_obj_t* sweep = nullptr;
  lv_obj_t* zoom_in = nullptr;
  lv_obj_t* zoom_out = nullptr;
  lv_obj_t* home = nullptr;

  lv_obj_t* card = nullptr;
  lv_obj_t* card_title = nullptr;
  lv_obj_t* card_type = nullptr;
  lv_obj_t* card_sub = nullptr;
  lv_obj_t* card_route = nullptr;    // "Dublin (DUB) > Lanzarote (ACE)"
  lv_obj_t* card_airline = nullptr;  // "Ryanair  -  Boeing 737-8AS"
  lv_obj_t* stat_title[6] = {};
  lv_obj_t* stat_value[6] = {};
  lv_obj_t* follow_btn = nullptr;
  lv_obj_t* fav_btn = nullptr;
  lv_obj_t* fav_img = nullptr;

  lv_obj_t* pick_bar = nullptr;
  lv_obj_t* pick_hint = nullptr;

  lv_timer_t* timer = nullptr;
  lv_timer_t* autohide_timer = nullptr;
};

Hud h;

/** Places a child centered at an angle (0 = north, clockwise) and radius fraction. */
void placePolar(lv_obj_t* o, float deg, float frac) {
  const float r = screenW() * 0.5f * frac;
  const float a = deg * 3.14159265f / 180.0f;
  const int32_t x = static_cast<int32_t>(std::sin(a) * r);
  const int32_t y = static_cast<int32_t>(-std::cos(a) * r);
  lv_obj_align(o, LV_ALIGN_CENTER, x, y);
}

void setHidden(lv_obj_t* o, bool hidden) {
  if (o == nullptr) return;
  if (hidden) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
}

void updateStatus() {
  const plat::WifiStatus ws = plat::wifiStatus();
  const app::FeedStatus fs = app::feedStatus();
  const Palette& p = pal();
  char txt[96];
  lv_color_t dot = p.accent;

  if (ws.state == plat::WifiState::Failed && std::strstr(ws.reason, "module") != nullptr) {
    std::snprintf(txt, sizeof(txt), LV_SYMBOL_WARNING "  %s", ws.reason);
    dot = p.danger;
  } else if (app::settings().wifi_ssid[0] == '\0') {
    std::snprintf(txt, sizeof(txt), LV_SYMBOL_WIFI "  Tap to set up Wi-Fi");
    dot = p.warn;
  } else if (ws.state == plat::WifiState::Connecting) {
    std::snprintf(txt, sizeof(txt), LV_SYMBOL_WIFI "  Connecting...");
    dot = p.warn;
  } else if (ws.state != plat::WifiState::Connected) {
    std::snprintf(txt, sizeof(txt), LV_SYMBOL_WARNING "  Wi-Fi: %s", ws.reason[0] ? ws.reason : "offline");
    dot = p.danger;
  } else if (!fs.ever_ok) {
    std::snprintf(txt, sizeof(txt), LV_SYMBOL_REFRESH "  %s", fs.error[0] ? fs.error : "Loading traffic...");
    dot = fs.error[0] ? p.danger : p.warn;
  } else {
    const uint32_t age_s = (plat::millis() - fs.last_ok_ms) / 1000;
    const size_t n = RadarView::get().visibleCount();
    if (!fs.last_ok && age_s > 20) {
      std::snprintf(txt, sizeof(txt), LV_SYMBOL_WARNING "  %u aircraft  -  data %us old", static_cast<unsigned>(n),
                    static_cast<unsigned>(age_s));
      dot = p.warn;
    } else {
      char r[20];
      app::formatDistance(RadarView::get().viewport().radiusKm(), r, sizeof(r));
      const int hidden = fs.last_total - fs.last_count;
      if (core::filtersActive(app::settings()) && hidden > 0) {
        std::snprintf(txt, sizeof(txt), "%u aircraft  (%d filtered)   " LV_SYMBOL_GPS " %s",
                      static_cast<unsigned>(n), hidden, r);
      } else {
        std::snprintf(txt, sizeof(txt), "%u aircraft   " LV_SYMBOL_GPS " %s", static_cast<unsigned>(n), r);
      }
    }
  }
  lv_label_set_text(h.status_label, txt);
  lv_obj_set_style_text_color(h.status_label, dot, 0);
  h.status_problem = !lv_color_eq(dot, p.accent);

  if (h.alt_key) {
    const auto& st = app::settings();
    setHidden(h.alt_key, st.color_mode != core::ColorMode::Altitude || st.theme == core::Theme::Night);
    lv_obj_invalidate(h.alt_key);  // colors follow the Custom theme
  }

  if (h.mute_label) {
    lv_label_set_text(h.mute_label, app::settings().sound_enabled ? LV_SYMBOL_VOLUME_MAX : LV_SYMBOL_MUTE);
  }

  if (h.clock) {
    if (app::settings().clock_mode) lv_obj_add_state(h.clock, LV_STATE_CHECKED);
    else lv_obj_remove_state(h.clock, LV_STATE_CHECKED);
  }

  // The filter button lights up while any filter hides aircraft.
  if (h.filter) {
    if (core::filtersActive(app::settings())) lv_obj_add_state(h.filter, LV_STATE_CHECKED);
    else lv_obj_remove_state(h.filter, LV_STATE_CHECKED);
  }

  RadarView& rv = RadarView::get();
  if (!rv.pickMode()) {
    // The Home button only matters when we're somewhere else.
    lv_obj_set_style_opa(h.home, rv.atHome() ? 110 : LV_OPA_COVER, 0);
  }
  if (h.sweep) {
    if (app::settings().sweep_enabled) lv_obj_add_state(h.sweep, LV_STATE_CHECKED);
    else lv_obj_remove_state(h.sweep, LV_STATE_CHECKED);
  }
}

/** Draws the altitude scale: a gradient bar with 0 / 10k / 20k / 30k / 40k ft below it. */
void onAltKeyDraw(lv_event_t* e) {
  lv_layer_t* layer = lv_event_get_layer(e);
  lv_obj_t* obj = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
  lv_area_t c;
  lv_obj_get_content_coords(obj, &c);
  const int32_t w = lv_area_get_width(&c);
  const int32_t bar_h = sc(8) > 4 ? sc(8) : 4;
  constexpr int kSteps = 40;
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  for (int i = 0; i < kSteps; ++i) {
    d.bg_color = altitudeColor(i * 40000 / (kSteps - 1), false);
    lv_area_t a = {c.x1 + w * i / kSteps, c.y1, c.x1 + w * (i + 1) / kSteps - 1, c.y1 + bar_h - 1};
    lv_draw_rect(layer, &d, &a);
  }
  const lv_font_t* f = sc(800) < 760 ? &lv_font_montserrat_14 : &lv_font_montserrat_16;
  static const char* const kLabels[] = {"0 ft", "10k", "20k", "30k", "40k"};
  lv_draw_label_dsc_t ld;
  lv_draw_label_dsc_init(&ld);
  ld.font = f;
  ld.color = pal().text_dim;
  for (int i = 0; i < 5; ++i) {
    ld.text = kLabels[i];
    lv_point_t sz;
    lv_text_get_size(&sz, kLabels[i], f, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    // End labels sit inside the bar's ends; the others are centered on their mark.
    const int32_t x = i == 0 ? c.x1 : (i == 4 ? c.x2 - sz.x : c.x1 + w * i / 4 - sz.x / 2);
    lv_area_t a = {x, c.y1 + bar_h + sc(3), x + sz.x + 1, c.y1 + bar_h + sc(3) + sz.y};
    lv_draw_label(layer, &ld, &a);
  }
}

void setStat(int i, const char* title, const char* value) {
  lv_label_set_text(h.stat_title[i], title);
  lv_label_set_text(h.stat_value[i], value);
}

void updateCard() {
  RadarView& rv = RadarView::get();
  const core::Track* t = rv.selected();
  const bool show = t != nullptr && !rv.pickMode();
  setHidden(h.card, !show);
  setHidden(h.zoom_in, show || rv.pickMode());
  setHidden(h.zoom_out, show || rv.pickMode());
  if (!show) return;

  const core::Aircraft& a = t->ac;
  lv_label_set_text(h.card_title, a.callsign[0] ? a.callsign : (a.reg[0] ? a.reg : a.hex));
  char badge[16];
  std::snprintf(badge, sizeof(badge), "%s%s", a.type[0] ? a.type : "?", a.military ? "  MIL" : "");
  lv_label_set_text(h.card_type, badge);

  // Airline and route (adsbdb), once looked up.
  {
    const app::RouteState rs = app::routeState();
    const bool mine = std::strcmp(rs.hex, a.hex) == 0;
    const core::route::Info& in = rs.info;
    if (mine && rs.status == app::RouteStatus::Loading) {
      lv_label_set_text(h.card_route, "");
      lv_label_set_text(h.card_airline, "Looking up airline and route...");
    } else if (mine && rs.status == app::RouteStatus::Ready) {
      char route[96] = "";
      if (in.has_route) {
        std::snprintf(route, sizeof(route), "%s (%s)  " LV_SYMBOL_RIGHT "  %s (%s)", in.origin.city, in.origin.code,
                      in.destination.city, in.destination.code);
      }
      lv_label_set_text(h.card_route, route);
      const char* who = in.airline[0] ? in.airline : in.owner;
      char line[96];
      std::snprintf(line, sizeof(line), "%s%s%s", who, who[0] && in.model[0] ? "  -  " : "", in.model);
      lv_label_set_text(h.card_airline, line);
    } else {
      lv_label_set_text(h.card_route, "");
      lv_label_set_text(h.card_airline, mine && rs.status == app::RouteStatus::Unknown ? "No airline or route info" : "");
    }
    setHidden(h.card_route, lv_label_get_text(h.card_route)[0] == '\0');
    setHidden(h.card_airline, lv_label_get_text(h.card_airline)[0] == '\0');
  }

  // Flight history status, once the download was asked for.
  char hist[32] = "";
  const app::TraceInfo ti = app::traceInfo();
  if (std::strcmp(ti.hex, a.hex) == 0) {
    switch (ti.status) {
      case app::TraceStatus::Loading: std::snprintf(hist, sizeof(hist), "  -  loading history..."); break;
      case app::TraceStatus::Unavailable: std::snprintf(hist, sizeof(hist), "  -  no history"); break;
      case app::TraceStatus::Ready: {
        const int min = static_cast<int>(ti.duration_s / 60.0f + 0.5f);
        if (min >= 60) std::snprintf(hist, sizeof(hist), "  -  history %dh %02dm", min / 60, min % 60);
        else std::snprintf(hist, sizeof(hist), "  -  history %d min", min);
        break;
      }
      default: break;
    }
  }
  char sub[96];
  std::snprintf(sub, sizeof(sub), "%s%s%s  %s%s%s", a.reg, a.reg[0] ? "  -  " : "", a.hex,
                a.category[0] ? "cat " : "", a.category, hist);
  lv_label_set_text(h.card_sub, sub);

  char v[40];
  if (a.on_ground) std::snprintf(v, sizeof(v), "Ground");
  else if (!a.alt_valid) std::snprintf(v, sizeof(v), "--");
  else std::snprintf(v, sizeof(v), "%d ft", static_cast<int>(a.alt_ft));
  setStat(0, "ALT", v);

  if (std::abs(a.vrate_fpm) < 100) std::snprintf(v, sizeof(v), "level");
  else std::snprintf(v, sizeof(v), "%s%d fpm", a.vrate_fpm > 0 ? "+" : "", static_cast<int>(a.vrate_fpm));
  setStat(1, "V/S", v);

  std::snprintf(v, sizeof(v), "%.0f kt", a.gs_kt);
  setStat(2, "SPEED", v);

  std::snprintf(v, sizeof(v), "%03.0f" "\xC2\xB0", a.track_deg);
  setStat(3, "TRACK", v);

  const auto& st = app::settings();
  const core::LatLon home{st.home_lat, st.home_lon};
  const core::LatLon pos = core::positionAt(*t, plat::millis());
  char d[20];
  app::formatDistance(core::distanceKm(home, pos), d, sizeof(d));
  std::snprintf(v, sizeof(v), "%s %03.0f" "\xC2\xB0", d, core::bearingDeg(home, pos));
  setStat(4, "FROM HOME", v);

  setStat(5, "SQUAWK", a.squawk[0] ? a.squawk : "--");
  lv_obj_set_style_text_color(h.card_title, a.emergency ? pal().danger : pal().text, 0);

  const bool fav = app::isFavorite(a.hex);
  lv_obj_set_style_image_recolor(h.fav_img, fav ? pal().favorite : pal().text_dim, 0);
  lv_obj_set_style_border_color(h.fav_btn, fav ? pal().favorite : pal().panel_border, 0);

  if (rv.following()) lv_obj_add_state(h.follow_btn, LV_STATE_CHECKED);
  else lv_obj_remove_state(h.follow_btn, LV_STATE_CHECKED);
}

void onTimer(lv_timer_t*) {
  updateStatus();
  updateCard();
  std::string notice;
  if (app::popNotice(&notice)) toast((std::string(LV_SYMBOL_BELL "  ") + notice).c_str(), 4000);
}

constexpr uint32_t kAutoHideIdleMs = 10000;
constexpr uint32_t kFadeMs = 400;

/**
 * Fades the buttons and the status pill out after 10 s without touches; any
 * touch brings them back.
 */
void onAutoHideTimer(lv_timer_t*) {
  const uint32_t idle = lv_display_get_inactive_time(nullptr);
  const uint32_t now = plat::millis();
  RadarView& rv = RadarView::get();
  // Keep them while an aircraft card or the home picker is open, or while the
  // status pill reports a problem (offline, feed errors).
  const bool may_hide = app::settings().autohide_buttons && rv.selected() == nullptr && !rv.pickMode() &&
                        !h.status_problem;

  if (h.controls_hidden) {
    if (idle < 200 || !may_hide) {
      h.controls_hidden = false;
      h.hide_done_ms = 0;
      lv_anim_delete(h.controls, nullptr);
      lv_obj_remove_flag(h.controls, LV_OBJ_FLAG_HIDDEN);
      lv_obj_fade_in(h.controls, kFadeMs / 2, 0);
    } else if (h.hide_done_ms != 0 && static_cast<int32_t>(now - h.hide_done_ms) >= 0) {
      // Fully transparent: hide it so invisible buttons can't be pressed.
      lv_obj_add_flag(h.controls, LV_OBJ_FLAG_HIDDEN);
      h.hide_done_ms = 0;
    }
  } else if (may_hide && idle >= kAutoHideIdleMs) {
    h.controls_hidden = true;
    h.hide_done_ms = now + kFadeMs;
    lv_obj_fade_out(h.controls, kFadeMs, 0);
  }
}

void buildCard(lv_obj_t* parent) {
  const Palette& p = pal();
  h.card = panel(parent);
  lv_obj_set_size(h.card, sc(520), LV_SIZE_CONTENT);
  lv_obj_align(h.card, LV_ALIGN_BOTTOM_MID, 0, -sc(126));
  lv_obj_set_style_pad_all(h.card, sc(16), 0);
  lv_obj_set_style_pad_hor(h.card, sc(22), 0);
  lv_obj_set_flex_flow(h.card, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_row(h.card, sc(6), 0);
  lv_obj_set_style_pad_column(h.card, 0, 0);
  lv_obj_add_flag(h.card, LV_OBJ_FLAG_CLICKABLE);  // don't let taps fall through to the map

  // Header row: callsign, type badge, follow + close buttons
  lv_obj_t* head = lv_obj_create(h.card);
  lv_obj_remove_style_all(head);
  lv_obj_set_size(head, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(head, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(head, sc(12), 0);

  h.card_title = label(head, "", font(FontSize::XL), p.text);
  h.card_type = label(head, "", font(FontSize::M), lv_color_black());
  lv_obj_set_style_bg_color(h.card_type, p.accent, 0);
  lv_obj_set_style_bg_opa(h.card_type, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(h.card_type, sc(8), 0);
  lv_obj_set_style_pad_hor(h.card_type, sc(8), 0);
  lv_obj_set_style_pad_ver(h.card_type, sc(2), 0);

  lv_obj_t* spacer = lv_obj_create(head);
  lv_obj_remove_style_all(spacer);
  lv_obj_set_flex_grow(spacer, 1);
  lv_obj_set_height(spacer, 1);

  // Favorite toggle: a round button holding the star image (no star glyph in the fonts).
  h.fav_btn = iconButton(head, "", sc(56), [] {
    const core::Track* t = RadarView::get().selected();
    if (t == nullptr) return;
    if (!app::isFavorite(t->ac.hex) && app::favorites().size() >= core::kMaxFavorites) {
      toast(LV_SYMBOL_WARNING "  Favorites full (32) - remove one first");
      return;
    }
    toast(app::toggleFavorite(t->ac) ? "Saved to favorites" : "Removed from favorites");
    updateCard();
  });
  h.fav_img = lv_image_create(h.fav_btn);
  lv_image_set_src(h.fav_img, sprites::star(sprites::starButtonSize()));
  lv_obj_center(h.fav_img);
  lv_obj_set_style_image_recolor_opa(h.fav_img, LV_OPA_COVER, 0);

  h.follow_btn = iconButton(head, LV_SYMBOL_GPS, sc(56), [] {
    RadarView& rv = RadarView::get();
    rv.setFollow(!rv.following());
    updateCard();
  });
  iconButton(head, LV_SYMBOL_CLOSE, sc(56), [] {
    RadarView::get().clearSelection();
    updateCard();
  });

  h.card_route = label(h.card, "", font(FontSize::M), p.text);
  lv_obj_set_width(h.card_route, lv_pct(100));
  lv_label_set_long_mode(h.card_route, LV_LABEL_LONG_DOT);
  h.card_airline = label(h.card, "", font(FontSize::S), p.accent);
  lv_obj_set_width(h.card_airline, lv_pct(100));
  lv_label_set_long_mode(h.card_airline, LV_LABEL_LONG_DOT);
  h.card_sub = label(h.card, "", font(FontSize::S), p.text_dim);
  lv_obj_set_width(h.card_sub, lv_pct(100));
  lv_label_set_long_mode(h.card_sub, LV_LABEL_LONG_DOT);

  for (int i = 0; i < 6; ++i) {
    lv_obj_t* cell = lv_obj_create(h.card);
    lv_obj_remove_style_all(cell);
    lv_obj_set_size(cell, lv_pct(33), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_COLUMN);
    h.stat_title[i] = label(cell, "", font(FontSize::S), p.text_dim);
    h.stat_value[i] = label(cell, "", font(FontSize::M), p.text);
  }
  setHidden(h.card, true);
}

void leavePickMode() {
  RadarView::get().setPickMode(false);
  setHidden(h.pick_bar, true);
  setHidden(h.pick_hint, true);
  setHidden(h.home, false);
  setHidden(h.gear, false);
  setHidden(h.sweep, false);
  setHidden(h.filter, false);
  setHidden(h.mute, false);
  setHidden(h.clock, false);
  setHidden(h.sleep, false);
  updateCard();
}

void buildPickMode(lv_obj_t* parent) {
  const Palette& p = pal();
  h.pick_hint = panel(parent);
  lv_obj_set_size(h.pick_hint, sc(460), LV_SIZE_CONTENT);
  lv_obj_align(h.pick_hint, LV_ALIGN_TOP_MID, 0, sc(150));
  lv_obj_t* l = label(h.pick_hint, "Drag the map so the crosshair\nis on your location", font(FontSize::M), p.text);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(l);

  h.pick_bar = lv_obj_create(parent);
  lv_obj_remove_style_all(h.pick_bar);
  lv_obj_set_size(h.pick_bar, sc(460), LV_SIZE_CONTENT);
  lv_obj_align(h.pick_bar, LV_ALIGN_BOTTOM_MID, 0, -sc(110));
  lv_obj_set_flex_flow(h.pick_bar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(h.pick_bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  pillButton(h.pick_bar, "Cancel", [] { leavePickMode(); });
  pillButton(
      h.pick_bar, LV_SYMBOL_HOME "  Set home",
      [] {
        RadarView& rv = RadarView::get();
        auto& st = app::settings();
        st.home_lat = rv.viewport().center().lat;
        st.home_lon = rv.viewport().center().lon;
        st.home_set = true;
        st.home_radius_km = static_cast<float>(rv.viewport().radiusKm());
        app::settingsChanged();
        app::settingsFlush();
        rv.markStaticDirty();
        leavePickMode();
        toast(LV_SYMBOL_OK "  Home location saved");
      },
      true);
  setHidden(h.pick_bar, true);
  setHidden(h.pick_hint, true);
}

}  // namespace

void enterPickMode() {
  RadarView::get().setPickMode(true);
  setHidden(h.pick_bar, false);
  setHidden(h.pick_hint, false);
  setHidden(h.home, true);
  setHidden(h.gear, true);
  setHidden(h.sweep, true);
  setHidden(h.filter, true);
  setHidden(h.mute, true);
  setHidden(h.clock, true);
  setHidden(h.sleep, true);
  updateCard();
}

void exitPickMode() { leavePickMode(); }

void create(lv_obj_t* parent) {
  const Palette& p = pal();
  h = Hud{};
  h.root = parent;

  // Full-screen, non-clickable layer: taps between buttons fall through to the map.
  h.controls = lv_obj_create(parent);
  lv_obj_remove_style_all(h.controls);
  lv_obj_set_size(h.controls, screenW(), screenH());
  lv_obj_remove_flag(h.controls, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(h.controls, LV_OBJ_FLAG_SCROLLABLE);

  // Status pill (top)
  h.status = panel(h.controls);  // fades with the buttons
  lv_obj_set_size(h.status, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_hor(h.status, sc(22), 0);
  lv_obj_set_style_pad_ver(h.status, sc(8), 0);
  lv_obj_set_style_radius(h.status, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(h.status, 190, 0);
  lv_obj_align(h.status, LV_ALIGN_TOP_MID, 0, sc(82));
  lv_obj_add_flag(h.status, LV_OBJ_FLAG_CLICKABLE);
  h.status_label = label(h.status, "", font(FontSize::S), p.text);
  onClick(h.status, [] {
    const plat::WifiStatus ws = plat::wifiStatus();
    openSettings(ws.state == plat::WifiState::Connected ? Page::About : Page::Wifi);
  });

  // Tiny altitude color key under the pill (fades with it).
  h.alt_key = lv_obj_create(h.controls);
  lv_obj_remove_style_all(h.alt_key);
  lv_obj_set_size(h.alt_key, sc(330), sc(50));
  lv_obj_align(h.alt_key, LV_ALIGN_TOP_MID, 0, sc(134));
  lv_obj_set_style_bg_color(h.alt_key, pal().panel, 0);
  lv_obj_set_style_bg_opa(h.alt_key, 170, 0);
  lv_obj_set_style_radius(h.alt_key, sc(12), 0);
  lv_obj_set_style_pad_hor(h.alt_key, sc(14), 0);
  lv_obj_set_style_pad_ver(h.alt_key, sc(7), 0);
  lv_obj_remove_flag(h.alt_key, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(h.alt_key, onAltKeyDraw, LV_EVENT_DRAW_MAIN, nullptr);

  const int32_t d = sc(76);
  h.gear = iconButton(h.controls, LV_SYMBOL_SETTINGS, d, [] { openSettings(Page::Main); });
  placePolar(h.gear, 45, 0.80f);

  // Quick access to Filters, between the gear and the "E" of the compass.
  h.filter = iconButton(h.controls, "", d, [] { openSettings(Page::Filters); });
  lv_obj_t* funnel = lv_image_create(h.filter);
  lv_image_set_src(funnel, sprites::funnel(sc(30) | 1));
  lv_obj_center(funnel);
  lv_obj_set_style_image_recolor(funnel, p.text, 0);
  lv_obj_set_style_image_recolor_opa(funnel, LV_OPA_COVER, 0);
  placePolar(h.filter, 77, 0.80f);

  // Clock face on/off, below the filter button.
  h.clock = iconButton(h.controls, "", d, [] {
    auto& st = app::settings();
    st.clock_mode = !st.clock_mode;
    app::settingsChanged();
    RadarView::get().markStaticDirty();
    updateStatus();
  });
  {
    lv_obj_t* icon = lv_image_create(h.clock);
    lv_image_set_src(icon, sprites::clockIcon(sc(32) | 1));
    lv_obj_center(icon);
    lv_obj_set_style_image_recolor(icon, p.text, 0);
    lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, 0);
  }
  placePolar(h.clock, 103, 0.80f);

  // Sleep, mirroring the clock button: screen off and quiet until a tap
  // (or an alarm, with Wake on alarm on).
  h.sleep = iconButton(h.controls, LV_SYMBOL_POWER, d, [] { app::setSleeping(true); });
  placePolar(h.sleep, -103, 0.80f);

  // Mute, mirroring the filter button on the left.
  h.mute = iconButton(h.controls, LV_SYMBOL_VOLUME_MAX, d, [] {
    auto& st = app::settings();
    st.sound_enabled = !st.sound_enabled;
    app::settingsChanged();
    if (st.sound_enabled) app::sound(core::Sound::Click);
    toast(st.sound_enabled ? LV_SYMBOL_VOLUME_MAX "  Sound on" : LV_SYMBOL_MUTE "  Sound off");
    updateStatus();
  });
  h.mute_label = lv_obj_get_child(h.mute, 0);
  placePolar(h.mute, -77, 0.80f);

  h.sweep = iconButton(h.controls, LV_SYMBOL_LOOP, d, [] {
    auto& st = app::settings();
    st.sweep_enabled = !st.sweep_enabled;
    app::settingsChanged();
    updateStatus();
  });
  placePolar(h.sweep, -45, 0.80f);

  h.zoom_in = iconButton(h.controls, LV_SYMBOL_PLUS, d, [] { RadarView::get().zoomBy(2.0); });
  placePolar(h.zoom_in, 135, 0.80f);
  h.zoom_out = iconButton(h.controls, LV_SYMBOL_MINUS, d, [] { RadarView::get().zoomBy(0.5); });
  placePolar(h.zoom_out, 225, 0.80f);

  h.home = iconButton(h.controls, LV_SYMBOL_HOME, sc(84), [] { RadarView::get().goHome(); });
  placePolar(h.home, 180, 0.82f);

  buildCard(parent);
  buildPickMode(parent);

  RadarView& rv = RadarView::get();
  rv.on_selection_changed = [] { updateCard(); };
  rv.on_long_press = [](core::LatLon ll) {
    static char body[96];
    std::snprintf(body, sizeof(body), "Make this spot your home location?\n%.4f, %.4f", ll.lat, ll.lon);
    confirm("Set home here?", body, LV_SYMBOL_HOME " Set home", [ll] {
      auto& st = app::settings();
      st.home_lat = ll.lat;
      st.home_lon = ll.lon;
      st.home_set = true;
      app::settingsChanged();
      app::settingsFlush();
      RadarView::get().markStaticDirty();
      toast(LV_SYMBOL_OK "  Home location saved");
    });
  };

  h.timer = lv_timer_create(onTimer, 500, nullptr);
  h.autohide_timer = lv_timer_create(onAutoHideTimer, 100, nullptr);
  onTimer(nullptr);
}

void destroy() {
  if (h.timer) lv_timer_delete(h.timer);
  if (h.autohide_timer) lv_timer_delete(h.autohide_timer);
  h = Hud{};
}

}  // namespace ui::hud
