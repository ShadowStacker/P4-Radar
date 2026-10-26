#include "ui/theme.h"

namespace ui {

namespace {

Palette s_pal;
int32_t s_w = 800;
int32_t s_h = 800;

lv_color_t hex(uint32_t c) { return lv_color_hex(c); }

Palette makePalette(core::Theme t) {
  Palette p;
  // Shared defaults
  p.label = hex(0xF2F5F7);
  p.label_dim = hex(0x9FB3C0);
  p.vector = hex(0xE0E0E0);
  p.home = hex(0xFFFFFF);
  p.select = hex(0xFFFFFF);
  p.text = hex(0xEAF2F5);
  p.text_dim = hex(0x8EA3AE);
  p.warn = hex(0xFFB020);
  p.danger = hex(0xFF3B3B);

  switch (t) {
    case core::Theme::Amber:
      p.bg = hex(0x0D0802);
      p.bg_center = hex(0x1A1004);
      p.grid = hex(0x5C3A08);
      p.grid_text = hex(0xB07A20);
      p.bezel = hex(0x7A4E0C);
      p.bezel_text = hex(0xE0A040);
      p.sweep = hex(0xFFB030);
      p.aircraft = hex(0xFFD070);
      p.trail = hex(0xC08020);
      p.runway = hex(0x8A6A3A);
      p.runway_label = hex(0xD0A060);
      p.panel = hex(0x1E1408);
      p.panel_border = hex(0x5C3A08);
      p.accent = hex(0xFFB030);
      p.label = hex(0xFFE2A8);
      p.label_dim = hex(0xC09050);
      break;
    case core::Theme::Ocean:
      p.bg = hex(0x020A1A);
      p.bg_center = hex(0x061830);
      p.grid = hex(0x10406A);
      p.grid_text = hex(0x4F8FC0);
      p.bezel = hex(0x1A5A8A);
      p.bezel_text = hex(0x8CC8F0);
      p.sweep = hex(0x40C8FF);
      p.aircraft = hex(0xFF4A4A);
      p.trail = hex(0x40A0D0);
      p.runway = hex(0x3896AA);
      p.runway_label = hex(0x6ED2E6);
      p.panel = hex(0x0A1C34);
      p.panel_border = hex(0x1A4A78);
      p.accent = hex(0x40C8FF);
      break;
    case core::Theme::Night:
      // Red-on-black: preserves night vision on a bedside table.
      p.bg = hex(0x000000);
      p.bg_center = hex(0x080000);
      p.grid = hex(0x401010);
      p.grid_text = hex(0x903030);
      p.bezel = hex(0x501414);
      p.bezel_text = hex(0xA03838);
      p.sweep = hex(0xC02020);
      p.aircraft = hex(0xFF5050);
      p.trail = hex(0x902020);
      p.vector = hex(0xA04040);
      p.runway = hex(0x602020);
      p.runway_label = hex(0x903030);
      p.panel = hex(0x140404);
      p.panel_border = hex(0x401010);
      p.accent = hex(0xD03030);
      p.label = hex(0xE04848);
      p.label_dim = hex(0x903030);
      p.text = hex(0xE04848);
      p.text_dim = hex(0x903030);
      p.home = hex(0xD04040);
      p.select = hex(0xFF6060);
      break;
    case core::Theme::Phosphor:
    default:
      p.bg = hex(0x020D06);
      p.bg_center = hex(0x05200E);
      p.grid = hex(0x0F4F22);
      p.grid_text = hex(0x3FA860);
      p.bezel = hex(0x17703A);
      p.bezel_text = hex(0x7CE89A);
      p.sweep = hex(0x40FF80);
      p.aircraft = hex(0xFFFFFF);
      p.trail = hex(0x40C070);
      p.runway = hex(0x3B8F8F);
      p.runway_label = hex(0x6FD0D0);
      p.panel = hex(0x0A1E12);
      p.panel_border = hex(0x1C5A30);
      p.accent = hex(0x40FF80);
      break;
  }
  return p;
}

}  // namespace

const Palette& pal() { return s_pal; }

void setTheme(core::Theme t) { s_pal = makePalette(t); }

void setScreenSize(int32_t w, int32_t h) {
  s_w = w;
  s_h = h;
}
int32_t screenW() { return s_w; }
int32_t screenH() { return s_h; }

int32_t sc(int32_t px) {
  const int32_t m = s_w < s_h ? s_w : s_h;
  return (px * m + 400) / 800;
}

lv_color_t altitudeColor(int32_t alt_ft, bool on_ground) {
  if (on_ground) return lv_color_hex(0x9A9A9A);
  // Hue stops (degrees) vs altitude, interpolated linearly.
  static const struct {
    int32_t alt;
    int32_t hue;
  } kStops[] = {{0, 20}, {2000, 35}, {6000, 85}, {10000, 140}, {20000, 195}, {30000, 250}, {40000, 300}};
  constexpr int n = sizeof(kStops) / sizeof(kStops[0]);
  int32_t hue = kStops[n - 1].hue;
  if (alt_ft <= kStops[0].alt) {
    hue = kStops[0].hue;
  } else {
    for (int i = 1; i < n; ++i) {
      if (alt_ft <= kStops[i].alt) {
        const int32_t a0 = kStops[i - 1].alt, a1 = kStops[i].alt;
        hue = kStops[i - 1].hue + (kStops[i].hue - kStops[i - 1].hue) * (alt_ft - a0) / (a1 - a0);
        break;
      }
    }
  }
  return lv_color_hsv_to_rgb(static_cast<uint16_t>(hue), 75, 100);
}

const lv_font_t* font(FontSize s) {
  // Montserrat sizes compiled in via sdkconfig (CONFIG_LV_FONT_MONTSERRAT_*).
  const bool small_panel = sc(800) < 760;
  switch (s) {
    case FontSize::S: return small_panel ? &lv_font_montserrat_16 : &lv_font_montserrat_18;
    case FontSize::M: return small_panel ? &lv_font_montserrat_20 : &lv_font_montserrat_22;
    case FontSize::L: return small_panel ? &lv_font_montserrat_24 : &lv_font_montserrat_26;
    case FontSize::XL: return small_panel ? &lv_font_montserrat_28 : &lv_font_montserrat_32;
    case FontSize::XXL:
    default: return small_panel ? &lv_font_montserrat_40 : &lv_font_montserrat_44;
  }
}

void applyLvglTheme(lv_display_t* disp) {
  lv_theme_t* th = lv_theme_default_init(disp, s_pal.accent, s_pal.warn, true, font(FontSize::M));
  lv_display_set_theme(disp, th);
}

}  // namespace ui
