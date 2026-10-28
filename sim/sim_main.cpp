// Headless simulator: runs the real UI against fake traffic, drives it with
// scripted touches (tap, drag, pinch) and writes PNG screenshots.
//
//   ./plane_radar_sim [out_dir] [--size 720]

#include <zlib.h>

#include <chrono>
#include <cmath>
#include <malloc.h>

#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "app/app.h"
#include "app/maps.h"
#include "core/clock.h"
#include "platform/platform.h"
#include "core/filter.h"
#include "core/traffic.h"
#include "fake_traffic.h"
#include "lvgl.h"
#include "sim_state.h"
#include "ui/display_rotation.h"
#include "ui/hud.h"
#include "ui/keyboard.h"
#include "ui/radar_view.h"
#include "ui/sprites.h"
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

int g_rotate = 0;  // quarter turns, from --rotate 90|180|270

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

std::function<void()> g_step_hook;  // bench: runs before every step

/** Advances virtual time, feeding traffic and running LVGL. */
void run(uint32_t ms, uint32_t step = 16) {
  for (uint32_t t = 0; t < ms; t += step) {
    sim::g_ms += step;
    if (g_step_hook) g_step_hook();
    g_traffic->advance(step / 1000.0);
    feedTraffic();
    if (!app::serviceUpdate() && !app::serviceFavoriteLookup() && !app::serviceRoute()) app::serviceTrace();
    if (!app::maps::serviceSd() && !app::serviceSdFiles()) app::maps::service(true);
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

/** The settings screen's scrolling list: its tallest child. */
lv_obj_t* settingsList() {
  lv_obj_t* scr = lv_screen_active();
  lv_obj_t* best = scr;
  for (uint32_t i = 0; i < lv_obj_get_child_count(scr); ++i) {
    lv_obj_t* c = lv_obj_get_child(scr, static_cast<int32_t>(i));
    if (lv_obj_get_height(c) > lv_obj_get_height(best) || best == scr) best = c;
  }
  return best;
}

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

/** Timing at several zooms with heavy traffic: --bench. Host-CPU numbers;
 *  compare them with each other, not with the device. */
int runBench() {
  sim::g_preset_wifi = true;
  app::init();
  auto& st = app::settings();
  sim::FakeTraffic traffic({st.home_lat, st.home_lon}, 7, 450, 420);
  g_traffic = &traffic;
  ui::init(g_disp);
  run(2000);
  auto& rv = ui::RadarView::get();
  std::printf("%-6s %-7s %6s %6s %8s %9s %9s %8s\n", "map", "radius", "blips", "labels", "frame_us",
              "static_us", "overlay_us", "lvgl_us");
  // BENCH_MAP / BENCH_RADIUS limit the run to one case (for profilers).
  const int only_map = std::getenv("BENCH_MAP") ? std::atoi(std::getenv("BENCH_MAP")) : -1;
  const double only_r = std::getenv("BENCH_RADIUS") ? std::atof(std::getenv("BENCH_RADIUS")) : -1;
  for (int map = 0; map <= 2; ++map) {
    if (only_map >= 0 && map != only_map) continue;
    for (double r : {10.0, 40.0, 100.0, 200.0, 400.0}) {
      if (only_r > 0 && r != only_r) continue;
      st.map_style = static_cast<uint8_t>(map);
      st.home_radius_km = r;
      rv.goHome(false);
      run(8000);  // tiles and traffic settle
      rv.stats() = {};
      g_render_ms_total = 0;
      g_render_count = 0;
      g_step_hook = [&rv] { rv.markStaticDirty(); };  // measure a static render every frame
      run(3000);
      g_step_hook = nullptr;
      const auto& x = rv.stats();
      const unsigned f = x.frames ? x.frames : 1;
      std::printf("%-6d %-7.0f %6u %6u %8llu %9llu %9llu %8.0f\n", map, r, x.blips / f, x.labels / f,
                  static_cast<unsigned long long>(x.frame_us / f),
                  static_cast<unsigned long long>(x.static_renders ? x.static_us / x.static_renders : 0),
                  static_cast<unsigned long long>(x.overlay_us / f), g_render_ms_total * 1000 / std::max(1, g_render_count));
    }
    if (only_r > 0) continue;
    // A pinch out while zoomed out (busy frames: static layer every frame).
    st.home_radius_km = 100;
    rv.goHome(false);
    run(6000);
    rv.stats() = {};
    g_render_ms_total = 0;
    g_render_count = 0;
    pinch(S(400), S(400), S(220), S(60), 40);
    const auto& x = rv.stats();
    const unsigned f = x.frames ? x.frames : 1;
    std::printf("%-6d %-7s %6u %6u %8llu %9llu %9llu %8.0f  (pinch)\n", map, "pinch", x.blips / f, x.labels / f,
                static_cast<unsigned long long>(x.frame_us / f),
                static_cast<unsigned long long>(x.static_renders ? x.static_us / x.static_renders : 0),
                static_cast<unsigned long long>(x.overlay_us / f), g_render_ms_total * 1000 / std::max(1, g_render_count));
  }
  return 0;
}

/** Memory over a long session of typical use: --soak [cycles]. Prints heap in
 *  use after each round; a leak shows as steady growth. */
int runSoak(int cycles) {
  sim::g_preset_wifi = true;
  app::init();
  auto& st = app::settings();
  sim::FakeTraffic traffic({st.home_lat, st.home_lon}, 7, 200, 300);
  g_traffic = &traffic;
  sim::g_trace_server = [](const std::string& hex) { return g_traffic->traceJson(hex); };
  sim::g_route_server = [](const std::string& hex) { return g_traffic->routeJson(hex); };
  ui::init(g_disp);
  run(5000);
  auto& rv = ui::RadarView::get();
  const ui::Page pages[] = {ui::Page::Main,      ui::Page::Display, ui::Page::Sound,      ui::Page::Traffic,
                            ui::Page::Filters,   ui::Page::Favorites, ui::Page::About,    ui::Page::CustomTheme,
                            ui::Page::Clock,     ui::Page::TimeZone, ui::Page::SdCard,    ui::Page::Location};
  for (int c = 0; c < cycles; ++c) {
    // Menus: every page, scrolled, then closed (alternating keep / discard).
    for (ui::Page pg : pages) {
      ui::closeSettings();
      run(200);
      ui::openSettings(pg);
      run(200);
      drag(S(400), S(600), S(400), S(250), 12);
      run(300);
    }
    st.volume = static_cast<uint8_t>(40 + c % 20);
    app::settingsChanged();
    ui::closeSettings(c % 2 == 1);
    run(500);
    // Radar: tap an aircraft (trail + route), zoom out and in, pan, home.
    int ax, ay;
    if (findAircraftOnScreen(&ax, &ay)) {
      tap(ax, ay);
      run(2500);
    }
    rv.clearSelection();
    pinch(S(400), S(400), S(220), S(60), 20);
    run(1500);
    pinch(S(400), S(400), S(60), S(220), 20);
    run(1500);
    drag(S(400), S(400), S(520), S(330), 10);
    run(1500);
    rv.goHome(false);
    // Theme and map changes rebuild screens.
    st.theme = static_cast<core::Theme>(c % 4);
    st.map_style = static_cast<uint8_t>(c % 4);
    ui::rebuild();
    run(4000);
    const struct mallinfo2 mi = mallinfo2();
    std::printf("soak %3d: heap in use %8.1f KB, %zu chunks\n", c, mi.uordblks / 1024.0, mi.ordblks);
    std::fflush(stdout);
  }
  return 0;
}

int main(int argc, char** argv) {
  bool bench = false;
  int soak = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--bench") == 0) {
      bench = true;
      continue;
    }
    if (std::strcmp(argv[i], "--soak") == 0) {
      soak = i + 1 < argc && std::atoi(argv[i + 1]) > 0 ? std::atoi(argv[++i]) : 40;
      continue;
    }
    if (std::strcmp(argv[i], "--size") == 0 && i + 1 < argc) g_size = std::atoi(argv[++i]);
    else if (std::strcmp(argv[i], "--rotate") == 0 && i + 1 < argc) g_rotate = std::atoi(argv[++i]) / 90;
    else sim::g_out_dir = argv[i];
  }
  std::string mk = "mkdir -p '" + sim::g_out_dir + "'";
  if (std::system(mk.c_str()) != 0) return 1;
  std::string p = g_size == 800 ? "" : std::to_string(g_size) + "_";
  if (g_rotate) p = "rot" + std::to_string(g_rotate * 90) + "_" + p;

  g_fb.assign(g_size * g_size, 0);
  lv_init();
  lv_tick_set_cb(tickCb);

  g_disp = lv_display_create(g_size, g_size);
  // Partial rendering in 80-line stripes, like the device.
  static std::vector<uint8_t> buf(g_size * 80 * 2);
  lv_display_set_buffers(g_disp, buf.data(), nullptr, static_cast<uint32_t>(buf.size()),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(g_disp, flushCb);
  // Screenshots then show the panel as mounted (taps stay in UI coordinates).
  ui::installDisplayRotation(g_disp, g_rotate);

  lv_indev_t* indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touchRead);

  if (bench) return runBench();
  if (soak) return runSoak(soak);

  sim::g_preset_wifi = false;  // start like a fresh device
  // A blank card: no settings.txt or favorites.txt from an earlier run.
  std::remove((std::string(plat::sdRoot()) + "/settings.txt").c_str());
  std::remove((std::string(plat::sdRoot()) + "/favorites.txt").c_str());
  app::init();
  sim::FakeTraffic traffic({app::settings().home_lat, app::settings().home_lon}, 42);
  g_traffic = &traffic;
  sim::g_trace_server = [](const std::string& hex) { return g_traffic->traceJson(hex); };
  sim::g_route_server = [](const std::string& hex) { return g_traffic->routeJson(hex); };

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
    // Zoom out to see the whole downloaded flight history.
    ui::RadarView::get().zoomBy(0.15);
    run(1500);
    shot((p + "35_flight_history").c_str());
    ui::RadarView::get().zoomBy(1.0 / 0.15);
    run(1500);
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
  lv_obj_scroll_to_y(settingsList(), ui::sc(760), LV_ANIM_OFF);
  run(300);
  shot((p + "27_settings_sound_favorites").c_str());
  ui::closeSettings();
  run(300);

  // Filters page (top, then scrolled).
  ui::openSettings(ui::Page::Filters);
  run(500);
  shot((p + "28_settings_filters").c_str());
  lv_obj_scroll_to_y(settingsList(), ui::sc(900), LV_ANIM_OFF);
  run(300);
  shot((p + "29_settings_filters_more").c_str());
  ui::closeSettings();
  run(300);

  // Helicopters only.
  app::settings().category_mask = core::kCatRotor;
  app::traffic().clear();
  g_next_fetch = 0;
  run(2000);
  shot((p + "30_filter_helicopters").c_str());
  app::settings().category_mask = 0xFF;
  app::traffic().clear();
  g_next_fetch = 0;
  run(2000);

  // Sweep variants.
  app::settings().sweep_trail_deg = 0;
  run(400);
  shot((p + "31_sweep_clean_bar").c_str());
  app::settings().sweep_trail_deg = 160;
  app::settings().sweep_smooth = true;
  run(400);
  shot((p + "32_sweep_smooth_long").c_str());
  app::settings().sweep_trail_deg = 70;
  app::settings().sweep_smooth = false;

  // Custom theme: settings page, then the radar in those colors with bigger
  // aircraft and labels.
  app::settings().theme = core::Theme::Custom;
  app::settings().custom_hue = 200;
  app::settings().custom_sat = 85;
  app::settings().custom_aircraft_hue = 45;
  app::settings().custom_aircraft_sat = 70;
  app::settings().color_mode = core::ColorMode::Classic;
  ui::rebuild();
  ui::openSettings(ui::Page::CustomTheme);
  run(600);
  shot((p + "36_settings_custom_theme").c_str());
  ui::closeSettings();
  app::settings().aircraft_scale = 130;
  app::settings().label_size = 2;
  ui::sprites::init(app::settings().aircraft_scale);
  ui::rebuild();
  run(1500);
  shot((p + "37_custom_theme_radar").c_str());
  app::settings().theme = core::Theme::Phosphor;
  app::settings().color_mode = core::ColorMode::Altitude;
  app::settings().aircraft_scale = 100;
  app::settings().label_size = 1;
  ui::sprites::init(100);
  ui::rebuild();
  run(300);

  // X discards: change a few things, close with X, check they're back.
  {
    const core::Settings before = app::settings();
    const size_t favs_before = app::favorites().size();
    ui::openSettings(ui::Page::Display);
    run(300);
    app::settings().theme = core::Theme::Amber;
    app::settings().units = core::Units::Km;
    app::settings().brightness = 30;
    app::settingsChanged();
    ui::rebuild();
    if (!app::favorites().empty()) app::removeFavorite(app::favorites()[0].hex);
    run(300);
    ui::closeSettings(true);
    run(800);
    const auto& st = app::settings();
    const bool ok = st.theme == before.theme && st.units == before.units && st.brightness == before.brightness &&
                    app::favorites().size() == favs_before;
    std::printf("discard check: %s\n", ok ? "OK" : "FAILED");
    if (!ok) return 1;
    shot((p + "38_changes_discarded").c_str());
  }

  // Sleep: screen off, then one tap on top of the gear button only wakes it.
  {
    app::setSleeping(true);
    run(600);
    shot((p + "50_sleeping").c_str());
    tap(S(626), S(174));  // where the gear button is
    run(600);
    const bool ok = !app::sleeping() && !ui::settingsOpen();
    std::printf("sleep check: %s\n", ok ? "OK" : "FAILED");
    if (!ok) return 1;
  }

  // Add a favorite by tail number.
  {
    std::vector<core::Track> tracks;
    app::traffic().snapshot(tracks);
    std::string reg, hex;
    for (const auto& t : tracks) {
      if (t.ac.reg[0] && !app::isFavorite(t.ac.hex) && !t.ac.military) {
        reg = t.ac.reg;
        hex = t.ac.hex;
        break;
      }
    }
    ui::openSettings(ui::Page::Favorites);
    run(300);
    app::lookupFavorite(reg);
    run(1200);
    const bool added = app::isFavorite(hex.c_str());
    std::printf("favorite by tail number (%s): %s\n", reg.c_str(), added ? "OK" : "FAILED");
    if (!added) return 1;
    shot((p + "44_favorite_added_by_tail").c_str());
    ui::closeSettings();
    app::removeFavorite(hex.c_str());
    run(300);
  }

  // Clock face, clock menus, and an alarm going off.
  {
    auto& st = app::settings();
    st.clock_mode = true;
    ui::RadarView::get().markStaticDirty();
    run(1200);
    shot((p + "39_clock_face").c_str());
    ui::openSettings(ui::Page::Clock);
    run(500);
    shot((p + "40_settings_clock").c_str());
    ui::closeSettings();
    ui::openSettings(ui::Page::TimeZone);
    run(400);
    shot((p + "41_settings_time_zone").c_str());
    ui::closeSettings();

    // Alarm one minute from now.
    const std::tm t = core::localTime(plat::unixTimeMs() / 1000 + 60);
    st.alarms[0].enabled = true;
    st.alarms[0].hour = static_cast<uint8_t>(t.tm_hour);
    st.alarms[0].minute = static_cast<uint8_t>(t.tm_min);
    st.alarms[0].days = 0x7F;
    app::settingsChanged();
    ui::openSettings(ui::Page::Clock);
    run(300);
    lv_obj_send_event(lv_obj_get_child(settingsList(), 6), LV_EVENT_CLICKED, nullptr);  // first alarm row
    run(400);
    shot((p + "42_settings_alarm_edit").c_str());
    ui::closeSettings();
    run(62000);
    const bool rang = app::alarmRinging() == 0;
    std::printf("alarm check: %s\n", rang ? "OK" : "FAILED");
    if (!rang) return 1;
    shot((p + "43_alarm_ringing").c_str());
    app::snoozeAlarm();
    run(300);
    const bool snoozed = app::alarmRinging() < 0 && app::snoozeMinutesLeft() == core::kSnoozeMinutes;
    std::printf("snooze check: %s\n", snoozed ? "OK" : "FAILED");
    if (!snoozed) return 1;
    app::stopAlarm();

    // Asleep: with "Wake on alarm" off (default) the alarm passes silently and
    // the screen stays off; with it on, the alarm rings and wakes the screen.
    for (int wake = 0; wake < 2; ++wake) {
      st.wake_on_alarm = wake == 1;
      const std::tm next = core::localTime(plat::unixTimeMs() / 1000 + 60);
      st.alarms[0].hour = static_cast<uint8_t>(next.tm_hour);
      st.alarms[0].minute = static_cast<uint8_t>(next.tm_min);
      app::setSleeping(true);
      run(62000);
      const bool ok = wake ? (app::alarmRinging() == 0 && !app::sleeping())
                           : (app::alarmRinging() < 0 && app::sleeping());
      std::printf("wake on alarm %s check: %s\n", wake ? "on" : "off", ok ? "OK" : "FAILED");
      if (!ok) return 1;
      app::stopAlarm();
      app::setSleeping(false);
      run(300);
    }
    st.wake_on_alarm = false;
    st.alarms[0].enabled = false;
    st.clock_mode = false;
    ui::RadarView::get().markStaticDirty();
    run(300);
  }

  // Central Oregon: home at Redmond, zoomed in to show the small fields.
  app::settings().home_lat = 44.2541;
  app::settings().home_lon = -121.1500;
  app::settings().home_radius_km = 30;
  ui::RadarView::get().goHome(false);
  ui::RadarView::get().markStaticDirty();
  run(800);
  shot((p + "33_central_oregon_30km").c_str());
  app::settings().home_radius_km = 12;
  ui::RadarView::get().goHome(false);
  run(800);
  shot((p + "34_central_oregon_12km").c_str());

  // Background maps over Central Oregon, saved to the (simulated) SD card.
  {
    auto& st = app::settings();
    st.home_radius_km = 40;
    ui::RadarView::get().goHome(false);
    st.map_style = 1;  // dark streets without a CARTO key: OpenStreetMap, darkened
    run(4000);
    shot((p + "45_map_dark_streets").c_str());
    std::snprintf(st.carto_key, sizeof(st.carto_key), "sim-key");  // with a key: CARTO
    run(4000);
    shot((p + "45b_map_dark_streets_carto").c_str());
    st.carto_key[0] = '\0';
    tap(S(525), S(695));  // the map button
    run(500);
    shot((p + "51_map_picker").c_str());
    lv_obj_clean(lv_layer_top());
    st.autohide_buttons = true;  // an earlier scene turned it off
    run(11000);  // buttons, status and map credit fade out
    shot((p + "52_map_buttons_hidden").c_str());
    tap(S(400), S(300));
    run(600);
    st.map_style = 2;  // topo, tinted
    run(4000);
    shot((p + "46_map_topo_tinted").c_str());
    st.map_tint = false;
    st.map_brightness = 55;
    run(3000);
    shot((p + "47_map_topo_original").c_str());
    st.map_style = 3;  // FAA sectional: always its own colors
    st.map_tint = true;
    st.map_brightness = 70;
    run(4000);
    shot((p + "53_map_faa_sectional").c_str());
    st.map_style = 2;
    st.map_tint = false;
    st.map_brightness = 55;
    run(3000);
    // Pan: the picture moves with the finger before the new tiles are composed.
    drag(S(400), S(400), S(520), S(330), 10);
    run(2500);
    shot((p + "48_map_after_pan").c_str());
    ui::openSettings(ui::Page::SdCard);
    run(1500);
    shot((p + "49_settings_sd_card").c_str());
    const app::maps::SdStatus sd = app::maps::sdStatus();
    std::printf("sd check: %s (%u tiles, %llu bytes)\n", sd.counted && sd.tiles > 10 ? "OK" : "FAILED",
                static_cast<unsigned>(sd.tiles), static_cast<unsigned long long>(sd.tile_bytes));
    if (!(sd.counted && sd.tiles > 10)) return 1;
    ui::closeSettings();
    st.map_style = 0;
    st.map_tint = true;
    run(300);
  }

  // settings.txt and favorites.txt: written by the radar, edited on a
  // computer, read back when the card is mounted again.
  {
    const std::string root = plat::sdRoot();
    auto readAll = [](const std::string& path) {
      std::string out;
      if (FILE* f = std::fopen(path.c_str(), "rb")) {
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
        std::fclose(f);
      }
      return out;
    };
    app::settings().volume = 42;
    app::settingsChanged();
    app::settingsFlush();
    run(500);
    std::string text = readAll(root + "/settings.txt");
    const bool written = text.find("volume = 42") != std::string::npos &&
                         text.find("wifi_ssid = \"HomeNet\"") != std::string::npos;
    std::printf("settings.txt written: %s\n", written ? "OK" : "FAILED");
    if (!written) return 1;

    // Pick a tail number to add by hand.
    std::vector<core::Track> tracks;
    app::traffic().snapshot(tracks);
    std::string reg, hex;
    for (const auto& t : tracks) {
      if (t.ac.reg[0] && !app::isFavorite(t.ac.hex) && !t.ac.military) {
        reg = t.ac.reg;
        hex = t.ac.hex;
        break;
      }
    }
    app::maps::sdRequest(app::maps::SdAction::Unmount);
    run(300);
    auto replace = [&](const std::string& from, const std::string& to) {
      const size_t at = text.find(from);
      if (at != std::string::npos) text.replace(at, from.size(), to);
    };
    replace("theme = phosphor", "theme = amber");
    replace("volume = 42", "volume = 75");
    replace("clock_mode = off", "clock_mode = on");
    if (FILE* f = std::fopen((root + "/settings.txt").c_str(), "wb")) {
      std::fwrite(text.data(), 1, text.size(), f);
      std::fclose(f);
    }
    std::string favs = readAll(root + "/favorites.txt");
    favs += "a0b1c2 TEST1 C172\n" + reg + "\n";
    if (FILE* f = std::fopen((root + "/favorites.txt").c_str(), "wb")) {
      std::fwrite(favs.data(), 1, favs.size(), f);
      std::fclose(f);
    }
    app::maps::sdRequest(app::maps::SdAction::Mount);
    run(3000);
    const auto& st = app::settings();
    const bool loaded = st.theme == core::Theme::Amber && st.volume == 75 && st.clock_mode;
    std::printf("settings.txt loaded: %s\n", loaded ? "OK" : "FAILED");
    if (!loaded) return 1;
    const bool fav_ok = app::isFavorite("a0b1c2") && app::isFavorite(hex.c_str());
    favs = readAll(root + "/favorites.txt");
    const bool fav_file_ok = favs.find(hex) != std::string::npos && favs.find("not looked up") == std::string::npos;
    std::printf("favorites.txt loaded (%s): %s, rewritten with hex: %s\n", reg.c_str(), fav_ok ? "OK" : "FAILED",
                fav_file_ok ? "OK" : "FAILED");
    if (!fav_ok || !fav_file_ok) return 1;
    shot((p + "54_settings_from_sd").c_str());
  }

  // Firmware update from the About page (GitHub and the installer are faked).
  {
    ui::openSettings(ui::Page::About);
    run(300);
    app::checkForUpdate();
    run(1500);
    const app::UpdateStatus u = app::updateStatus();
    const bool found = u.state == app::UpdateState::Available && std::strcmp(u.latest, "v1.13.0") == 0;
    std::printf("update check: %s\n", found ? "OK" : "FAILED");
    if (!found) return 1;
    lv_obj_scroll_to_y(lv_obj_get_child(lv_screen_active(), 3), S(260), LV_ANIM_OFF);
    run(300);
    shot((p + "55_update_available").c_str());
    app::installUpdate();
    run(600);
    const bool installed = app::updateStatus().state == app::UpdateState::Installed;
    std::printf("update install: %s\n", installed ? "OK" : "FAILED");
    if (!installed) return 1;
    shot((p + "56_update_installed").c_str());
    run(3000);  // restarts (the simulator just logs it)
  }

  std::printf("frames: %d, avg lv_timer_handler %.2f ms (host CPU)\n", g_render_count,
              g_render_ms_total / std::max(1, g_render_count));
  return 0;
}
