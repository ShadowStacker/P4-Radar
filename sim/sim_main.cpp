// Headless simulator: runs the real UI against fake traffic, drives it with
// scripted touches (tap, drag, pinch) and writes PNG screenshots.
//
//   ./plane_radar_sim [out_dir] [--size 720]

#include <zlib.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "app/app.h"
#include "core/traffic.h"
#include "fake_traffic.h"
#include "lvgl.h"
#include "sim_state.h"
#include "ui/hud.h"
#include "ui/keyboard.h"
#include "ui/radar_view.h"
#include "ui/theme.h"
#include "ui/ui.h"

namespace {

int g_size = 800;
std::vector<uint16_t> g_fb;
lv_display_t* g_disp = nullptr;
sim::FakeTraffic* g_traffic = nullptr;
uint32_t g_next_fetch = 0;
double g_render_ms_total = 0;
int g_render_count = 0;

uint32_t tickCb() { return sim::g_ms; }

void flushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* px) {
  const int w = lv_area_get_width(area);
  const auto* src = reinterpret_cast<const uint16_t*>(px);
  for (int y = area->y1; y <= area->y2; ++y) {
    std::memcpy(&g_fb[y * g_size + area->x1], src, w * 2);
    src += w;
  }
  lv_display_flush_ready(disp);
}

void touchRead(lv_indev_t*, lv_indev_data_t* data) {
  data->state = sim::g_touch.down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
  data->point.x = sim::g_touch.x[0];
  data->point.y = sim::g_touch.y[0];
}

void pngChunk(FILE* f, const char* type, const std::vector<uint8_t>& data) {
  auto be32 = [f](uint32_t v) {
    uint8_t b[4] = {static_cast<uint8_t>(v >> 24), static_cast<uint8_t>(v >> 16), static_cast<uint8_t>(v >> 8),
                    static_cast<uint8_t>(v)};
    std::fwrite(b, 1, 4, f);
  };
  be32(static_cast<uint32_t>(data.size()));
  std::fwrite(type, 1, 4, f);
  if (!data.empty()) std::fwrite(data.data(), 1, data.size(), f);
  uLong crc = crc32(0, reinterpret_cast<const Bytef*>(type), 4);
  if (!data.empty()) crc = crc32(crc, data.data(), static_cast<uInt>(data.size()));
  be32(static_cast<uint32_t>(crc));
}

/** Writes the framebuffer as PNG, blacking out pixels outside the round panel. */
void writePng(const std::string& path) {
  std::vector<uint8_t> raw;
  raw.reserve((g_size * 3 + 1) * g_size);
  const float c = (g_size - 1) / 2.0f, r = g_size / 2.0f;
  for (int y = 0; y < g_size; ++y) {
    raw.push_back(0);
    for (int x = 0; x < g_size; ++x) {
      const uint16_t p = g_fb[y * g_size + x];
      uint8_t rr = static_cast<uint8_t>(((p >> 11) & 0x1F) * 255 / 31);
      uint8_t gg = static_cast<uint8_t>(((p >> 5) & 0x3F) * 255 / 63);
      uint8_t bb = static_cast<uint8_t>((p & 0x1F) * 255 / 31);
      const float d = std::hypot(x - c, y - c);
      if (d > r) {
        rr = gg = bb = 0x16;  // bezel
      }
      raw.push_back(rr);
      raw.push_back(gg);
      raw.push_back(bb);
    }
  }
  uLongf zlen = compressBound(static_cast<uLong>(raw.size()));
  std::vector<uint8_t> z(zlen);
  compress2(z.data(), &zlen, raw.data(), static_cast<uLong>(raw.size()), 6);
  z.resize(zlen);

  FILE* f = std::fopen(path.c_str(), "wb");
  if (f == nullptr) {
    std::perror(path.c_str());
    return;
  }
  const uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  std::fwrite(sig, 1, 8, f);
  std::vector<uint8_t> ihdr = {0, 0, 0, 0, 0, 0, 0, 0, 8, 2, 0, 0, 0};
  for (int i = 0; i < 4; ++i) {
    ihdr[i] = static_cast<uint8_t>(g_size >> (24 - 8 * i));
    ihdr[4 + i] = static_cast<uint8_t>(g_size >> (24 - 8 * i));
  }
  pngChunk(f, "IHDR", ihdr);
  pngChunk(f, "IDAT", z);
  pngChunk(f, "IEND", {});
  std::fclose(f);
}

void feedTraffic() {
  if (sim::g_ms < g_next_fetch) return;
  g_next_fetch = sim::g_ms + app::settings().poll_interval_s * 1000u;
  core::LatLon c;
  double r;
  app::queryArea(&c, &r);
  app::ingest(200, g_traffic->json(c, r * 1.25 + 5));
}

/** Advances virtual time, feeding traffic and running LVGL. */
void run(uint32_t ms, uint32_t step = 16) {
  for (uint32_t t = 0; t < ms; t += step) {
    sim::g_ms += step;
    g_traffic->advance(step / 1000.0);
    feedTraffic();
    const auto t0 = std::chrono::steady_clock::now();
    lv_timer_handler();
    g_render_ms_total += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    ++g_render_count;
  }
}

void shot(const char* name) {
  lv_obj_invalidate(lv_screen_active());
  lv_refr_now(g_disp);
  const std::string path = sim::g_out_dir + "/" + name + ".png";
  writePng(path);
  std::printf("[shot] %s\n", path.c_str());
}

void press(int x, int y) {
  sim::g_touch.down = true;
  sim::g_touch.count = 1;
  sim::g_touch.x[0] = static_cast<int16_t>(x);
  sim::g_touch.y[0] = static_cast<int16_t>(y);
}
void release() {
  sim::g_touch.down = false;
  sim::g_touch.count = 0;
}

void tap(int x, int y) {
  press(x, y);
  run(80);
  release();
  run(60);
}

void drag(int x0, int y0, int x1, int y1, int steps) {
  press(x0, y0);
  run(40);
  for (int i = 1; i <= steps; ++i) {
    press(x0 + (x1 - x0) * i / steps, y0 + (y1 - y0) * i / steps);
    run(16);
  }
  run(80);  // pause before lifting: no fling
  release();
  run(60);
}

/** Two-finger pinch around (cx, cy) from half-distance d0 to d1. */
void pinch(int cx, int cy, int d0, int d1, int steps) {
  press(cx - d0, cy);
  run(40);
  for (int i = 0; i <= steps; ++i) {
    const int d = d0 + (d1 - d0) * i / steps;
    sim::g_touch.down = true;
    sim::g_touch.count = 2;
    sim::g_touch.x[0] = static_cast<int16_t>(cx - d);
    sim::g_touch.y[0] = static_cast<int16_t>(cy + d / 4);
    sim::g_touch.x[1] = static_cast<int16_t>(cx + d);
    sim::g_touch.y[1] = static_cast<int16_t>(cy - d / 4);
    run(16);
  }
  release();
  run(60);
}

int S(int px_at_800) { return px_at_800 * g_size / 800; }

/** Screen position of the first visible airborne aircraft near the middle. */
bool findAircraftOnScreen(int* x, int* y) {
  std::vector<core::Track> tracks;
  app::traffic().snapshot(tracks);
  auto& vp = ui::RadarView::get().viewport();
  float best = 1e9f;
  for (const auto& t : tracks) {
    const core::PointF p = vp.project(core::positionAt(t, sim::g_ms));
    const float d = std::hypot(p.x - vp.cx() - S(120), p.y - vp.cy() + S(60));
    if (d < best && t.ac.category[1] != '7') {
      best = d;
      *x = static_cast<int>(p.x);
      *y = static_cast<int>(p.y);
    }
  }
  return best < 1e9f;
}

}  // namespace

int main(int argc, char** argv) {
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--size") == 0 && i + 1 < argc) g_size = std::atoi(argv[++i]);
    else sim::g_out_dir = argv[i];
  }
  std::string mk = "mkdir -p '" + sim::g_out_dir + "'";
  if (std::system(mk.c_str()) != 0) return 1;
  const std::string p = g_size == 800 ? "" : std::to_string(g_size) + "_";

  g_fb.assign(g_size * g_size, 0);
  lv_init();
  lv_tick_set_cb(tickCb);

  g_disp = lv_display_create(g_size, g_size);
  // Partial rendering in 80-line stripes, like the device.
  static std::vector<uint8_t> buf(g_size * 80 * 2);
  lv_display_set_buffers(g_disp, buf.data(), nullptr, static_cast<uint32_t>(buf.size()),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(g_disp, flushCb);

  lv_indev_t* indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touchRead);

  sim::g_preset_wifi = false;  // start like a fresh device
  app::init();
  sim::FakeTraffic traffic({app::settings().home_lat, app::settings().home_lon}, 42);
  g_traffic = &traffic;

  ui::init(g_disp);
  run(600);
  shot((p + "01_first_boot_wifi").c_str());

  ui::textInput("Password for HomeNet", "hunter2", ui::InputKind::Password, nullptr);
  run(300);
  shot((p + "02_wifi_password").c_str());
  lv_obj_clean(lv_layer_top());

  // "Connect" and go back to the radar.
  std::snprintf(app::settings().wifi_ssid, sizeof(app::settings().wifi_ssid), "HomeNet");
  ui::closeSettings();
  run(150000);  // 2.5 minutes of traffic so trails build up
  shot((p + "03_radar_home").c_str());

  if (g_size == 800) {
    // Sweep animation frames for a GIF.
    for (int i = 0; i < 24; ++i) {
      run(66);
      char name[32];
      std::snprintf(name, sizeof(name), "sweep_%02d", i);
      shot(name);
    }
  }

  int ax = 0, ay = 0;
  if (findAircraftOnScreen(&ax, &ay)) {
    tap(ax, ay);
    run(600);
    shot((p + "04_aircraft_selected").c_str());
  }
  ui::RadarView::get().clearSelection();
  run(600);

  // Pinch out (zoom in) near the bottom-left, where the arrivals are.
  pinch(S(330), S(470), S(50), S(210), 24);
  run(3000);
  shot((p + "05_pinch_zoom").c_str());

  drag(S(400), S(400), S(560), S(300), 20);
  run(2500);
  shot((p + "06_panned_away").c_str());

  tap(S(400), S(400 + 328));  // Home button
  run(1500);
  shot((p + "07_back_home").c_str());

  ui::openSettings(ui::Page::Main);
  run(500);
  shot((p + "08_settings").c_str());
  const struct {
    ui::Page page;
    const char* name;
  } pages[] = {{ui::Page::Display, "09_settings_display"},
               {ui::Page::Sound, "10_settings_sound"},
               {ui::Page::Location, "11_settings_location"},
               {ui::Page::Traffic, "12_settings_traffic"},
               {ui::Page::About, "13_settings_about"}};
  for (const auto& pg : pages) {
    ui::closeSettings();
    run(300);
    ui::openSettings(pg.page);
    run(600);
    shot((p + pg.name).c_str());
  }
  ui::closeSettings();
  run(400);

  const struct {
    core::Theme theme;
    const char* name;
  } themes[] = {{core::Theme::Amber, "14_theme_amber"},
                {core::Theme::Ocean, "15_theme_ocean"},
                {core::Theme::Night, "16_theme_night"}};
  for (const auto& th : themes) {
    app::settings().theme = th.theme;
    ui::rebuild();
    run(1200);
    shot((p + th.name).c_str());
  }
  app::settings().theme = core::Theme::Phosphor;
  ui::rebuild();
  run(500);

  ui::hud::enterPickMode();
  run(500);
  shot((p + "17_pick_home").c_str());
  ui::hud::exitPickMode();

  traffic.setEmergency(true);
  run(6000);
  shot((p + "18_emergency").c_str());

  traffic.setEmergency(false);

  // Military-only filter.
  app::settings().military_only = true;
  traffic.recallMilitary();
  app::traffic().clear();
  g_next_fetch = 0;
  run(12000);
  shot((p + "19_military_only").c_str());
  if (findAircraftOnScreen(&ax, &ay)) {
    tap(ax, ay);
    run(600);
    shot((p + "20_military_selected").c_str());
  }
  ui::RadarView::get().clearSelection();
  app::settings().military_only = false;
  app::traffic().clear();
  g_next_fetch = 0;

  // Auto-hide: buttons fade after 10 s idle, any touch brings them back.
  app::settings().autohide_buttons = true;
  run(11500);
  shot((p + "21_buttons_hidden").c_str());
  tap(S(400), S(300));
  run(800);
  shot((p + "22_buttons_back").c_str());
  app::settings().autohide_buttons = false;

  // Favorites: save the aircraft nearest the middle, then see it come back into range.
  if (findAircraftOnScreen(&ax, &ay)) {
    tap(ax, ay);
    run(400);
    const core::Track* t = ui::RadarView::get().selected();
    if (t != nullptr) app::toggleFavorite(t->ac);
    run(700);
    shot((p + "23_favorite_card").c_str());
    ui::RadarView::get().clearSelection();
    run(2500);
    shot((p + "24_favorite_on_radar").c_str());
    // Forget everything and refetch: the favorite is "detected" again.
    app::traffic().clear();
    g_next_fetch = 0;
    run(1200);
    shot((p + "25_favorite_detected_notice").c_str());
  }
  ui::openSettings(ui::Page::Favorites);
  run(600);
  shot((p + "26_settings_favorites").c_str());
  ui::closeSettings();
  run(300);
  ui::openSettings(ui::Page::Sound);
  run(400);
  lv_obj_scroll_to_y(lv_obj_get_child(lv_screen_active(), -1), ui::sc(760), LV_ANIM_OFF);
  run(300);
  shot((p + "27_settings_sound_favorites").c_str());
  ui::closeSettings();
  run(300);

  std::printf("frames: %d, avg lv_timer_handler %.2f ms (host CPU)\n", g_render_count,
              g_render_ms_total / std::max(1, g_render_count));
  return 0;
}
