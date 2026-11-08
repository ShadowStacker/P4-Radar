// Headless simulator: runs the real UI against fake traffic, drives it with
// scripted touches (tap, drag, pinch) and writes PNG screenshots.
//
//   ./plane_radar_sim [out_dir] [--size 720]

#include <zlib.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include <cmath>
#include <malloc.h>

#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#if __has_include(<valgrind/callgrind.h>)
#include <valgrind/callgrind.h>
#endif

#include "app/app.h"
#include "app/mqtt_bridge.h"
#include "core/logbook.h"
#include "app/maps.h"
#include "app/web.h"
#include "core/png.h"
#include "core/clock.h"
#include "core/tiles.h"
#include "core/geo.h"
#include "platform/platform.h"
#include "core/filter.h"
#include "core/traffic.h"
#include "fake_traffic.h"
#include "lvgl.h"
#include "sim_state.h"
#include "ui/display_rotation.h"
#include "ui/hud.h"
#include "ui/keyboard.h"
#include "ui/settings_ui.h"
#include "ui/radar_view.h"
#include "ui/sprites.h"
#include "ui/theme.h"
#include "ui/ui.h"
#include "ui/whats_new.h"

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
  if (app::replayActive()) return;  // the replay fills the traffic instead
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
    // In the network task's order: card work first, then downloads.
    const bool card = app::maps::serviceSd() || app::serviceSdFiles() || app::serviceCardWork();
    while (app::serviceCardQueue()) {}
    app::serviceReplay();
    if (!app::serviceUpdate() && !app::serviceFavoriteLookup() && !app::servicePlaceSearch() && !app::serviceRoute()) {
      app::serviceTrace();
    }
    if (!card) app::maps::service(true);
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

/** True if a label under `obj` contains `needle` (the toasts on the top layer). */
lv_obj_t* findLabel(lv_obj_t* obj, const char* text) {
  if (lv_obj_check_type(obj, &lv_label_class) && std::strcmp(lv_label_get_text(obj), text) == 0) return obj;
  for (uint32_t i = 0; i < lv_obj_get_child_count(obj); ++i) {
    if (lv_obj_t* f = findLabel(lv_obj_get_child(obj, static_cast<int32_t>(i)), text)) return f;
  }
  return nullptr;
}

bool hasLabel(lv_obj_t* obj, const char* needle) {
  if (lv_obj_check_type(obj, &lv_label_class) && std::strstr(lv_label_get_text(obj), needle) != nullptr) return true;
  for (uint32_t i = 0; i < lv_obj_get_child_count(obj); ++i) {
    if (hasLabel(lv_obj_get_child(obj, static_cast<int32_t>(i)), needle)) return true;
  }
  return false;
}

/** A request to the phone page, answered while the UI keeps running (as on the device). */
plat::WebResponse web(const char* method, const std::string& path, const std::string& body = "",
                      const std::string& cookie = "") {
  plat::WebResponse out;
  std::atomic<bool> done{false};
  std::thread th([&] {
    plat::WebRequest r;
    r.method = method;
    const size_t q = path.find('?');
    r.path = path.substr(0, q);
    if (q != std::string::npos) r.query = path.substr(q + 1);
    r.body = body;
    r.content_length = body.size();
    r.cookie = cookie;
    out = sim::g_web_handler(r);
    done = true;
  });
  while (!done) run(16);
  th.join();
  return out;
}

std::string readFile(const std::string& path) {
  std::string out;
  if (FILE* f = std::fopen(path.c_str(), "rb")) {
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
    std::fclose(f);
  }
  return out;
}

}  // namespace


/**
 * Background map tiles must meet without a seam: at several zooms, the
 * screen along each tile edge may not be darker than the map next to it.
 * Returns the number of zooms with a seam.
 */
int mapSeamCheck(const std::string& p) {
  auto& st = app::settings();
  st.map_style = 4;  // satellite (the simulator draws it like topo: bright, detailed)
  st.map_tint = false;
  st.map_brightness = 100;
  st.show_rings = false;
  st.sweep_enabled = false;
  st.show_trails = false;
  st.show_runways = false;
  st.show_airport_labels = false;
  ui::RadarView& rv = ui::RadarView::get();
  int total = 0;
  for (double radius : {6.3, 11.7, 23.1, 37.9, 61.3, 97.7, 160.0}) {
    rv.viewport().setCenter({st.home_lat + 0.0123, st.home_lon - 0.0217});
    rv.viewport().setRadiusKm(radius);
    rv.markStaticDirty();
    const uint64_t cu0 = rv.mapLayer().compose_us_;
    const uint32_t cn0 = rv.mapLayer().composes_;
    run(6000);
    const uint32_t cn = rv.mapLayer().composes_ - cn0;
    std::printf("map compose at %.1f km: %u times, %.1f ms each\n", radius, static_cast<unsigned>(cn),
                cn ? (rv.mapLayer().compose_us_ - cu0) / 1000.0 / cn : 0.0);
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(g_disp);
    const core::Viewport& vp = rv.viewport();
    const int z = core::tiles::zoomFor(rv.mapVariant(), vp.kmPerPx(), vp.center().lat);
    auto lum = [](uint16_t c) { return ((c >> 11) & 31) * 2 + ((c >> 5) & 63) + (c & 31) * 2; };
    auto at = [](int x, int y) { return g_fb[static_cast<size_t>(y) * g_size + x]; };
    int seams = 0;
    const double n = std::ldexp(1.0, z);
    // Columns between tiles.
    const double x0 = (vp.center().lon + 180.0) / 360.0 * n;
    for (int k = -4; k <= 5; ++k) {
      const uint32_t tx = static_cast<uint32_t>(std::floor(x0)) + k;
      const float bx = vp.project({vp.center().lat, core::tiles::tileLon(tx, z)}).x;
      for (int col : {static_cast<int>(std::floor(bx)) - 1, static_cast<int>(std::floor(bx)), static_cast<int>(std::ceil(bx))}) {
        if (col < 3 || col > g_size - 4) continue;
        int line = 0;
        for (int y = g_size / 4; y < g_size * 3 / 4; ++y) {
          const int l = lum(at(col, y)), a = lum(at(col - 2, y)), b = lum(at(col + 2, y));
          if (std::min(a, b) > 40 && l * 3 < std::min(a, b) * 2) ++line;
        }
        seams = std::max(seams, line);
      }
    }
    // Rows between tiles.
    for (int k = -6; k <= 6; ++k) {
      double lat_y = 0;
      {
        const double lr = core::degToRad(vp.center().lat);
        lat_y = (1.0 - std::log(std::tan(lr) + 1.0 / std::cos(lr)) / M_PI) / 2.0 * n;
      }
      const double ty = std::floor(lat_y) + k;
      const float by = vp.project({core::tiles::tileLat(ty, z), vp.center().lon}).y;
      for (int row : {static_cast<int>(std::floor(by)) - 1, static_cast<int>(std::floor(by)), static_cast<int>(std::ceil(by))}) {
        if (row < 3 || row > g_size - 4) continue;
        int line = 0;
        for (int x = g_size / 4; x < g_size * 3 / 4; ++x) {
          const int l = lum(at(x, row)), a = lum(at(x, row - 2)), b = lum(at(x, row + 2));
          if (std::min(a, b) > 40 && l * 3 < std::min(a, b) * 2) ++line;
        }
        seams = std::max(seams, line);
      }
    }
    // Map details (contour lines, labels, aircraft) give short dark runs; a
    // seam runs the whole way along the tile edge.
    const bool seam = seams > g_size / 8;
    std::printf("map seams at %.1f km (zoom %d): longest dark run on a tile edge %d px: %s\n", radius, z, seams,
                seam ? "FAILED" : "OK");
    if (seam) {
      char name[64];
      std::snprintf(name, sizeof(name), "seams_%.0fkm", radius);
      shot((p + name).c_str());
    }
    total += seam ? 1 : 0;
  }
  return total;
}

/** Serves the phone page on http://localhost:8080 in real time: --web. */
int runWeb() {
  sim::g_preset_wifi = true;
  setenv("SIM_WEB", "1", 1);
  app::init();
  std::snprintf(app::settings().last_seen_version, sizeof(app::settings().last_seen_version), "%s",
                plat::firmwareVersion());  // no "What's new" popping up during the scenes
  auto& st = app::settings();
  st.home_set = true;
  sim::FakeTraffic traffic({st.home_lat, st.home_lon}, 42);
  g_traffic = &traffic;
  sim::g_route_server = [](const std::string& hex) { return g_traffic->routeJson(hex); };
  ui::init(g_disp);
  run(2000);
  std::printf("[sim] phone page: %s\n", app::web::address().c_str());
  std::fflush(stdout);
  for (;;) {
    run(16);
    std::this_thread::sleep_for(std::chrono::milliseconds(12));
  }
}

/** Timing at several zooms with heavy traffic: --bench. Host-CPU numbers;
 *  compare them with each other, not with the device. */
int runBench() {
  sim::g_preset_wifi = true;
  app::init();
  std::snprintf(app::settings().last_seen_version, sizeof(app::settings().last_seen_version), "%s",
                plat::firmwareVersion());  // no "What's new" popping up during the scenes
  auto& st = app::settings();
  sim::FakeTraffic traffic({st.home_lat, st.home_lon}, 7, 450, 420);
  g_traffic = &traffic;
  ui::init(g_disp);
  run(2000);
  auto& rv = ui::RadarView::get();
  std::printf("%-6s %-7s %6s %6s %8s %9s %9s %8s %9s %6s\n", "map", "radius", "blips", "labels", "frame_us",
              "static_us", "overlay_us", "lvgl_us", "update_us", "segs");
  // BENCH_MAP / BENCH_RADIUS limit the run to one case (for profilers).
  const int only_map = std::getenv("BENCH_MAP") ? std::atoi(std::getenv("BENCH_MAP")) : -1;
  if (std::getenv("BENCH_NO_TRAILS")) st.show_trails = false;
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
      const auto x = rv.stats();
      // A traffic update with trails on: just the trails redrawn (the base copied).
      rv.stats() = {};
      g_step_hook = [&rv] { rv.markTrailsDirty(); };
      run(3000);
      g_step_hook = nullptr;
      const auto& u = rv.stats();
      const unsigned f = x.frames ? x.frames : 1;
      std::printf("%-6d %-7.0f %6u %6u %8llu %9llu %9llu %8.0f %9llu %6u\n", map, r, x.blips / f, x.labels / f,
                  static_cast<unsigned long long>(x.frame_us / f),
                  static_cast<unsigned long long>(x.static_renders ? x.static_us / x.static_renders : 0),
                  static_cast<unsigned long long>(x.overlay_us / f), g_render_ms_total * 1000 / std::max(1, g_render_count),
                  static_cast<unsigned long long>(u.static_renders ? u.static_us / u.static_renders : 0),
                  static_cast<unsigned>(x.static_renders ? x.trail_segs / x.static_renders : 0));
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

/** Time to build each settings page and to scroll it: --pages. Host CPU. */
int runPages() {
  sim::g_preset_wifi = true;
  app::init();
  std::snprintf(app::settings().last_seen_version, sizeof(app::settings().last_seen_version), "%s",
                plat::firmwareVersion());
  auto& st = app::settings();
  sim::FakeTraffic traffic({st.home_lat, st.home_lon}, 7, 120, 300);
  g_traffic = &traffic;
  ui::init(g_disp);
  run(3000);
  std::function<uint32_t(lv_obj_t*)> count = [&](lv_obj_t* o) {
    uint32_t n = 1;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i) n += count(lv_obj_get_child(o, static_cast<int32_t>(i)));
    return n;
  };
  const ui::Page pages[] = {ui::Page::Main,    ui::Page::Display, ui::Page::Sound,   ui::Page::Traffic,
                            ui::Page::Filters, ui::Page::Updates, ui::Page::About,   ui::Page::Wifi,
                            ui::Page::Clock,   ui::Page::Map,     ui::Page::Sweep,   ui::Page::Power,
                            ui::Page::Aircraft, ui::Page::Nearby};
  double total_open = 0, total_scroll = 0;
  for (ui::Page pg : pages) {
    ui::closeSettings();
    run(300);
    double open_ms = 1e9;
    for (int k = 0; k < 5; ++k) {  // best of five: the host is noisy
      ui::closeSettings();
      run(200);
      auto t0 = std::chrono::steady_clock::now();
      ui::openSettings(pg);
      lv_refr_now(g_disp);
      open_ms = std::min(open_ms, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
    }
    g_render_ms_total = 0;
    g_render_count = 0;
    for (int k = 0; k < 6; ++k) {
      drag(S(400), S(650), S(400), S(250), 20);
      drag(S(400), S(250), S(400), S(650), 20);
    }
    run(800);
    const double scroll = g_render_ms_total / std::max(1, g_render_count);
    total_open += open_ms;
    total_scroll += scroll;
    std::printf("page %2d: open %6.2f ms, while scrolling %5.3f ms per frame, %u objects\n", static_cast<int>(pg),
                open_ms, scroll, static_cast<unsigned>(count(lv_screen_active())));
  }
  std::printf("total: open %.1f ms, scrolling %.3f ms per frame\n", total_open,
              total_scroll / (sizeof(pages) / sizeof(pages[0])));
  return 0;
}

/** One settings page opened and scrolled up and down (for instruction counts under valgrind): --scroll PAGE. */
int runScroll(int page) {
  sim::g_preset_wifi = true;
  app::init();
  std::snprintf(app::settings().last_seen_version, sizeof(app::settings().last_seen_version), "%s",
                plat::firmwareVersion());
  auto& st = app::settings();
  sim::FakeTraffic traffic({st.home_lat, st.home_lon}, 7, 120, 300);
  g_traffic = &traffic;
  ui::init(g_disp);
  run(1000);
  ui::openSettings(static_cast<ui::Page>(page));
  run(500);
#if __has_include(<valgrind/callgrind.h>)
  CALLGRIND_ZERO_STATS;  // count only the scrolling
#endif
  for (int k = 0; k < 4; ++k) {
    drag(S(400), S(650), S(400), S(250), 20);
    drag(S(400), S(250), S(400), S(650), 20);
  }
  run(500);
  return 0;
}

/** Memory over a long session of typical use: --soak [cycles]. Prints heap in
 *  use after each round; a leak shows as steady growth. */
int runSoak(int cycles) {
  sim::g_preset_wifi = true;
  app::init();
  std::snprintf(app::settings().last_seen_version, sizeof(app::settings().last_seen_version), "%s",
                plat::firmwareVersion());  // no "What's new" popping up during the scenes
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
  bool page_timing = false;
  bool web_mode = false;
  int scroll_page = -1;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--bench") == 0) {
      bench = true;
      continue;
    }
    if (std::strcmp(argv[i], "--scroll") == 0 && i + 1 < argc) {
      scroll_page = std::atoi(argv[++i]);
      continue;
    }
    if (std::strcmp(argv[i], "--web") == 0) {
      web_mode = true;
      continue;
    }
    if (std::strcmp(argv[i], "--pages") == 0) {
      page_timing = true;
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
  if (page_timing) return runPages();
  if (web_mode) return runWeb();
  if (scroll_page >= 0) return runScroll(scroll_page);

  sim::g_preset_wifi = false;  // start like a fresh device
  // A blank card: no settings.txt or favorites.txt from an earlier run.
  std::remove((std::string(plat::sdRoot()) + "/settings.txt").c_str());
  std::remove((std::string(plat::sdRoot()) + "/favorites.txt").c_str());
  // Screenshots are named by the (fixed) simulated time: start without old ones.
  if (std::system(("rm -rf '" + std::string(plat::sdRoot()) + "/screenshots'").c_str()) != 0) return 1;
  {
    // A logbook that began ten days ago (so "first time here" isn't still learning).
    const std::string root = plat::sdRoot();
    if (std::system(("mkdir -p '" + root + "'").c_str()) != 0) return 1;
    const int64_t then = plat::unixTimeMs() / 1000 - 10 * 86400;
    const std::tm t = core::localTime(then);
    if (FILE* f = std::fopen((root + "/logbook.csv").c_str(), "wb")) {
      std::fprintf(f, "%s%04d-%02d-%02d,12:00,a00001,OLD1,N1,C172,first,%lld\n", core::logbook::csvHeader(),
                   t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, static_cast<long long>(then));
      std::fclose(f);
    }
  }
  app::init();
  std::snprintf(app::settings().last_seen_version, sizeof(app::settings().last_seen_version), "%s",
                plat::firmwareVersion());  // no "What's new" popping up during the scenes
  app::settings().first_time_alert = false;  // (turned on in its own scene: every fake aircraft is new)
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
  if (std::getenv("SIM_ONLY_SEAMS")) return mapSeamCheck(p) == 0 ? 0 : 1;  // (debugging)

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
               {ui::Page::Aircraft, "79_settings_aircraft"},
               {ui::Page::Map, "80_settings_map"},
               {ui::Page::Sweep, "81_settings_sweep"},
               {ui::Page::Power, "82_settings_dimming_night"},
               {ui::Page::Sound, "10_settings_sound"},
               {ui::Page::Location, "11_settings_location"},
               {ui::Page::Traffic, "12_settings_traffic"},
               {ui::Page::HomeAssistant, "86_settings_home_assistant"},
               {ui::Page::About, "13_settings_about"}};
  for (const auto& pg : pages) {
    ui::closeSettings();
    run(300);
    ui::openSettings(pg.page);
    run(600);
    shot((p + pg.name).c_str());
    if (pg.page == ui::Page::Traffic) {
      // Your own receiver chosen: its address and fallback rows.
      core::Settings& st = app::settings();
      const core::DataSource was = st.source;
      st.source = core::DataSource::Local;
      std::snprintf(st.local_url, sizeof(st.local_url), "%s", "192.168.1.50");
      ui::closeSettings();
      run(300);
      ui::openSettings(pg.page);
      run(600);
      shot((p + "12_settings_traffic_receiver").c_str());
      st.source = was;
      st.local_url[0] = '\0';
    }
    if (pg.page == ui::Page::About) {
      // The memory graphs further down.
      if (lv_obj_t* l = findLabel(lv_screen_active(), "Icon cache")) lv_obj_scroll_to_view_recursive(l, LV_ANIM_OFF);
      run(300);
      shot((p + "13_settings_about_memory").c_str());
    }
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
    st.map_style = 5;  // street map: OSM in its own colors
    run(4000);
    shot((p + "61_map_street_names").c_str());
    st.map_style = 4;  // satellite
    run(4000);
    shot((p + "62_map_satellite").c_str());
    {
      const core::Settings before = st;
      const int bad = mapSeamCheck(p);
      // Tiles arriving one after another (a new map picture every 250 ms)
      // with the sweep off (10 fps): the background still shows them as it
      // goes, instead of starting over each time and showing nothing.
      ui::RadarView& rv = ui::RadarView::get();
      st.sweep_enabled = false;
      run(1000);
      const uint32_t shown0 = rv.totals().static_shown;
      for (int i = 0; i < 16; ++i) {
        rv.markStaticDirty();
        run(250);
      }
      const uint32_t shown = rv.totals().static_shown - shown0;
      std::printf("background while tiles arrive: shown %u times in 4 s: %s\n", static_cast<unsigned>(shown),
                  shown >= 3 ? "OK" : "FAILED");
      if (shown < 3) return 1;
      st = before;
      ui::RadarView::get().goHome(false);
      ui::RadarView::get().markStaticDirty();
      run(2000);
      if (bad > 0) return 1;
    }
    st.map_style = 3;
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

  // Night mode set to sleep: the night starts (screen off), then ends (awake).
  {
    auto& st = app::settings();
    const std::tm t = core::localTime(plat::unixTimeMs() / 1000);
    const int now_min = t.tm_hour * 60 + t.tm_min;
    st.night_dim = true;
    st.night_sleep = true;
    st.night_from_min = static_cast<uint16_t>((now_min + 1439) % 1440);
    st.night_to_min = static_cast<uint16_t>((now_min + 60) % 1440);
    run(6000);
    const bool asleep = app::sleeping();
    st.night_to_min = st.night_from_min;  // the night is over (empty window)
    run(6000);
    const bool awake = !app::sleeping();
    std::printf("night sleep: %s, morning wake: %s\n", asleep ? "OK" : "FAILED", awake ? "OK" : "FAILED");
    if (!asleep || !awake) return 1;
    st.night_dim = false;
    st.night_sleep = false;
    run(300);
  }

  // Daily log of the traffic near home, and the on-screen stats.
  {
    const core::daylog::Day today = app::dayLogToday();
    const bool logged = today.date != 0 && today.count() > 0;
    std::printf("daily log (%d aircraft): %s\n", today.count(), logged ? "OK" : "FAILED");
    if (!logged) return 1;
    ui::openSettings(ui::Page::DayLog);
    run(800);
    shot((p + "64_daily_log").c_str());
    ui::closeSettings();
    app::settings().show_stats = true;
    lv_display_trigger_activity(nullptr);
    run(2500);
    shot((p + "65_stats_overlay").c_str());
    {
      std::function<void(lv_obj_t*)> dump = [&](lv_obj_t* o) {
        if (lv_obj_check_type(o, &lv_label_class) && std::strstr(lv_label_get_text(o), "fps")) {
          std::printf("stats:\n%s\n", lv_label_get_text(o));
        }
        for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i) dump(lv_obj_get_child(o, static_cast<int32_t>(i)));
      };
      dump(lv_layer_sys());
      // Without the sweep: only the aircraft are redrawn.
      app::settings().sweep_enabled = false;
      run(2500);
      dump(lv_layer_sys());
      app::settings().sweep_enabled = true;
    }
    app::settings().show_stats = false;
    run(300);
  }

  // Nearby aircraft: the list is sorted, and a tap shows the aircraft on the radar.
  {
    ui::RadarView& rv = ui::RadarView::get();
    rv.clearSelection();
    rv.goHome(false);
    run(3000);
    std::vector<core::Track> tracks;
    app::traffic().snapshot(tracks);
    const auto& st = app::settings();
    const core::LatLon home{st.home_lat, st.home_lon};
    const core::Track* nearest = nullptr;
    const core::Track* highest = nullptr;
    for (const core::Track& t : tracks) {
      if (!nearest || core::distanceKm(home, {t.ac.lat, t.ac.lon}) < core::distanceKm(home, {nearest->ac.lat, nearest->ac.lon})) nearest = &t;
      if (!t.ac.on_ground && t.ac.alt_valid && (!highest || t.ac.alt_ft > highest->ac.alt_ft)) highest = &t;
    }
    auto name = [](const core::Track* t) {
      return std::string(t->ac.callsign[0] ? t->ac.callsign : (t->ac.reg[0] ? t->ac.reg : t->ac.hex));
    };
    // The first aircraft row: the first label after the section heading that names a tracked aircraft.
    auto firstRowName = [&]() -> std::string {
      std::string found;
      std::function<void(lv_obj_t*)> walk = [&](lv_obj_t* o) {
        if (!found.empty()) return;
        if (lv_obj_check_type(o, &lv_label_class)) {
          const char* tx = lv_label_get_text(o);
          for (const core::Track& t : tracks) {
            if (name(&t) == tx) {
              found = tx;
              return;
            }
          }
        }
        for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i) walk(lv_obj_get_child(o, static_cast<int32_t>(i)));
      };
      walk(lv_screen_active());
      return found;
    };
    // The airplane button on the radar (left side, mirroring the camera) opens it.
    lv_display_trigger_activity(nullptr);
    run(300);
    const float ang = -121.5f * 3.14159265f / 180.0f;
    const int bx = static_cast<int>(g_size / 2 + std::sin(ang) * g_size / 2 * 0.80f);
    const int by = static_cast<int>(g_size / 2 - std::cos(ang) * g_size / 2 * 0.80f);
    for (int i = 0; i < 2 && !ui::settingsOpen(); ++i) {  // the first tap may only bring the buttons back
      tap(bx, by);
      run(600);
    }
    const bool button = ui::settingsOpen() && hasLabel(lv_screen_active(), "Nearby aircraft");
    std::printf("nearby aircraft button: %s\n", button ? "OK" : "FAILED");
    if (!button) return 1;
    const std::string by_distance = firstRowName();
    shot((p + "87_nearby_aircraft").c_str());
    // Sort by altitude: tap the "Altitude" choice.
    {
      lv_obj_t* bm = nullptr;
      std::function<void(lv_obj_t*)> find = [&](lv_obj_t* o) {
        if (!bm && lv_obj_check_type(o, &lv_buttonmatrix_class)) bm = o;
        for (uint32_t i = 0; !bm && i < lv_obj_get_child_count(o); ++i) find(lv_obj_get_child(o, static_cast<int32_t>(i)));
      };
      find(lv_screen_active());
      if (bm) {
        lv_buttonmatrix_set_selected_button(bm, 1);  // "Altitude"
        lv_buttonmatrix_set_button_ctrl(bm, 1, LV_BUTTONMATRIX_CTRL_CHECKED);
        lv_obj_send_event(bm, LV_EVENT_VALUE_CHANGED, nullptr);
      }
    }
    run(600);
    const std::string by_altitude = firstRowName();
    const bool sorted = nearest && highest && by_distance == name(nearest) && by_altitude == name(highest);
    // Tap the first row: the radar shows it, selected.
    std::string picked = by_altitude;
    if (lv_obj_t* row = findLabel(lv_screen_active(), picked.c_str())) lv_obj_send_event(lv_obj_get_parent(row), LV_EVENT_CLICKED, nullptr);
    run(1500);
    const core::Track* sel = rv.selected();
    const bool shown = !ui::settingsOpen() && sel != nullptr && name(sel) == picked;
    shot((p + "88_nearby_selected").c_str());
    std::printf("nearby aircraft (%u): sorted %s (%s, %s), tap shows it %s\n", static_cast<unsigned>(tracks.size()),
                sorted ? "OK" : "FAILED", by_distance.c_str(), by_altitude.c_str(), shown ? "OK" : "FAILED");
    if (!sorted || !shown) return 1;
    rv.clearSelection();
    rv.goHome(false);
    run(1000);
  }

  // Bookmarks: the bookmark button on the radar opens the list (slots 1-4
  // and 5-8 both show); tapping one flies there at its zoom; a long press on
  // the radar offers "Set as home" or "Bookmark", and bookmarking names it.
  {
    ui::RadarView& rv = ui::RadarView::get();
    rv.goHome(false);
    run(500);
    std::vector<std::string> problems;
    app::applySettingsText("place1 = Cabin, 44.05, -121.31, 15\nplace5 = Lake, 43.70, -121.25, 30\n", &problems);
    lv_display_trigger_activity(nullptr);
    run(300);
    const float ang = -102.0f * 3.14159265f / 180.0f;
    const int bx = static_cast<int>(g_size / 2 + std::sin(ang) * g_size / 2 * 0.80f);
    const int by = static_cast<int>(g_size / 2 - std::cos(ang) * g_size / 2 * 0.80f);
    for (int i = 0; i < 2 && !ui::settingsOpen(); ++i) {  // the first tap may only bring the buttons back
      tap(bx, by);
      run(600);
    }
    const bool button = ui::settingsOpen() && ui::settings_ui::currentPage() == ui::Page::Bookmarks;
    const bool listed = hasLabel(lv_screen_active(), "Cabin") && hasLabel(lv_screen_active(), "Lake") &&
                        hasLabel(lv_screen_active(), "Bookmark the radar's view");
    shot((p + "80_bookmarks").c_str());
    lv_obj_t* row = findLabel(lv_screen_active(), "Lake");
    if (row) lv_obj_send_event(lv_obj_get_parent(row), LV_EVENT_CLICKED, nullptr);
    run(2000);
    const bool went = !ui::settingsOpen() && core::distanceKm(rv.viewport().center(), {43.70, -121.25}) < 1.0 &&
                      std::fabs(rv.viewport().radiusKm() - 30.0) < 0.5;
    std::printf("bookmarks: button %s, listed %s, go there %s\n", button ? "OK" : "FAILED", listed ? "OK" : "FAILED",
                went ? "OK" : "FAILED");
    if (!button || !listed || !went) return 1;

    // Long press on the radar, away from the buttons.
    rv.goHome(false);
    run(1500);
    lv_display_trigger_activity(nullptr);
    run(300);
    const int lx = g_size / 2 + g_size / 8, ly = g_size / 2 - g_size / 10;
    const core::LatLon spot = rv.viewport().unproject(static_cast<float>(lx), static_cast<float>(ly));
    press(lx, ly);
    run(1200);
    release();
    run(300);
    const bool asked = hasLabel(lv_layer_top(), "This spot") && hasLabel(lv_layer_top(), "Set as home") &&
                       findLabel(lv_layer_top(), "Bookmark") != nullptr;
    shot((p + "81_long_press").c_str());
    if (lv_obj_t* b = findLabel(lv_layer_top(), "Bookmark")) lv_obj_send_event(lv_obj_get_parent(b), LV_EVENT_CLICKED, nullptr);
    run(400);
    lv_obj_t* kb = nullptr;
    std::function<void(lv_obj_t*)> findKb = [&](lv_obj_t* o) {
      if (!kb && lv_obj_check_type(o, &lv_keyboard_class)) kb = o;
      for (uint32_t i = 0; !kb && i < lv_obj_get_child_count(o); ++i) findKb(lv_obj_get_child(o, static_cast<int32_t>(i)));
    };
    findKb(lv_layer_top());
    const bool named_prompt = kb != nullptr && hasLabel(lv_layer_top(), "blank: Bookmark 3");
    if (kb) {
      lv_textarea_set_text(lv_keyboard_get_textarea(kb), "Trailhead");
      lv_obj_send_event(kb, LV_EVENT_READY, nullptr);
    }
    run(600);
    const core::Place& added = core::bookmark(app::settings(), 1);  // the first free slot
    const bool saved = std::strcmp(added.name, "Trailhead") == 0 && core::distanceKm({added.lat, added.lon}, spot) < 0.5;
    // "Set as home" from a second long press.
    const core::Settings before = app::settings();
    press(lx, ly);
    run(1200);
    release();
    run(300);
    if (lv_obj_t* b = findLabel(lv_layer_top(), LV_SYMBOL_HOME "  Set as home")) {
      lv_obj_send_event(lv_obj_get_parent(b), LV_EVENT_CLICKED, nullptr);
    }
    run(400);
    const bool home = core::distanceKm({app::settings().home_lat, app::settings().home_lon}, spot) < 0.5;
    std::printf("long press: asked %s, name prompt %s, bookmarked %s, set home %s\n", asked ? "OK" : "FAILED",
                named_prompt ? "OK" : "FAILED", saved ? "OK" : "FAILED", home ? "OK" : "FAILED");
    if (!asked || !named_prompt || !saved || !home) return 1;
    // Back to how the other scenes expect it.
    app::settings().home_lat = before.home_lat;
    app::settings().home_lon = before.home_lon;
    for (int i = 0; i < core::kMaxBookmarks; ++i) core::bookmark(app::settings(), i) = core::Place();
    app::settingsChanged();
    rv.markStaticDirty();
    rv.goHome(false);
    run(2000);
  }

  // Place search: the search bar on the Location page, results from the
  // (fake) search service, a result shown on the radar; nothing found.
  {
    ui::RadarView& rv = ui::RadarView::get();
    ui::openSettings(ui::Page::Location);
    run(500);
    const bool bar = hasLabel(lv_screen_active(), "Search a city, state or country");
    if (lv_obj_t* l = findLabel(lv_screen_active(), "Search a city, state or country")) {
      lv_obj_send_event(lv_obj_get_parent(l), LV_EVENT_CLICKED, nullptr);
    }
    run(400);
    auto typeAndSearch = [&](const char* text) {
      lv_obj_t* kb = nullptr;
      std::function<void(lv_obj_t*)> findKb = [&](lv_obj_t* o) {
        if (!kb && lv_obj_check_type(o, &lv_keyboard_class)) kb = o;
        for (uint32_t i = 0; !kb && i < lv_obj_get_child_count(o); ++i) findKb(lv_obj_get_child(o, static_cast<int32_t>(i)));
      };
      findKb(lv_layer_top());
      if (kb == nullptr) return false;
      lv_textarea_set_text(lv_keyboard_get_textarea(kb), text);
      lv_obj_send_event(kb, LV_EVENT_READY, nullptr);
      for (int i = 0; i < 40 && app::placeSearch().state == app::PlaceSearchState::Searching; ++i) run(100);
      run(800);  // the page notices and rebuilds
      return true;
    };
    const bool typed = typeAndSearch("Bend");
    const bool found = ui::settings_ui::currentPage() == ui::Page::PlaceSearch &&
                       hasLabel(lv_screen_active(), "Deschutes County, Oregon") &&
                       hasLabel(lv_screen_active(), "Monterey County") && hasLabel(lv_screen_active(), "city");
    shot((p + "82_place_search").c_str());
    if (lv_obj_t* l = findLabel(lv_screen_active(), "Deschutes County, Oregon, United States")) {
      lv_obj_send_event(lv_obj_get_parent(lv_obj_get_parent(l)), LV_EVENT_CLICKED, nullptr);
    }
    run(300);
    const bool asked = hasLabel(lv_layer_top(), "Show on radar") && hasLabel(lv_layer_top(), "Set as home");
    if (lv_obj_t* l = findLabel(lv_layer_top(), "Show on radar")) lv_obj_send_event(lv_obj_get_parent(l), LV_EVENT_CLICKED, nullptr);
    run(2500);
    const bool shown = !ui::settingsOpen() && core::distanceKm(rv.viewport().center(), {44.058, -121.315}) < 1.0 &&
                       rv.viewport().radiusKm() > 6 && rv.viewport().radiusKm() < 12;
    // Nothing found.
    ui::openSettings(ui::Page::PlaceSearch);
    run(400);
    if (lv_obj_t* l = findLabel(lv_screen_active(), "Search a city, state or country")) {
      lv_obj_send_event(lv_obj_get_parent(l), LV_EVENT_CLICKED, nullptr);
    }
    run(400);
    typeAndSearch("Xyzzy");
    const bool none = hasLabel(lv_screen_active(), "Nothing found");
    std::printf("place search: bar %s, typed %s, results %s, asked %s, shown %s, nothing found %s\n", bar ? "OK" : "FAILED",
                typed ? "OK" : "FAILED", found ? "OK" : "FAILED", asked ? "OK" : "FAILED", shown ? "OK" : "FAILED",
                none ? "OK" : "FAILED");
    if (!bar || !typed || !found || !asked || !shown || !none) return 1;
    ui::closeSettings();
    rv.goHome(false);
    run(2000);
  }

  // Your own sounds: a WAV in sounds/ on the card replaces a built-in sound;
  // a broken one is reported; deleting them brings the built-in ones back.
  {
    const std::string dir = std::string(plat::sdRoot()) + "/sounds";
    std::filesystem::create_directories(dir);
    {
      // 0.5 s of a 440 Hz tone, 16-bit stereo at 44.1 kHz.
      std::string data;
      for (int i = 0; i < 22050; ++i) {
        const int16_t v = static_cast<int16_t>(std::lround(std::sin(i * 2 * 3.14159265 * 440 / 44100) * 12000));
        for (int c = 0; c < 2; ++c) data.append(reinterpret_cast<const char*>(&v), 2);
      }
      auto le = [](uint32_t v, int n) {
        std::string o;
        for (int i = 0; i < n; ++i) o.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
        return o;
      };
      const std::string wav = "RIFF" + le(36 + data.size(), 4) + "WAVEfmt " + le(16, 4) + le(1, 2) + le(2, 2) +
                              le(44100, 4) + le(44100 * 4, 4) + le(4, 2) + le(16, 2) + "data" + le(data.size(), 4) + data;
      std::ofstream(dir + "/first_time.wav", std::ios::binary) << wav;
      std::ofstream(dir + "/alarm.wav", std::ios::binary) << "this is not a sound";
    }
    app::reloadSounds();
    run(300);
    std::vector<std::string> lines = app::customSounds();
    auto has = [&](const char* needle) {
      for (const std::string& l : lines) {
        if (l.find(needle) != std::string::npos) return true;
      }
      return false;
    };
    const bool loaded = has("first_time.wav: OK, 0.5 s") && has("alarm.wav: not a WAV file") && lines.size() == 2;
    ui::openSettings(ui::Page::Sound);
    run(500);
    const bool listed = hasLabel(lv_screen_active(), "first_time.wav: OK") && hasLabel(lv_screen_active(), "Load them again");
    if (lv_obj_t* l = findLabel(lv_screen_active(), "YOUR OWN SOUNDS (SD CARD)")) lv_obj_scroll_to_view_recursive(l, LV_ANIM_OFF);
    run(300);
    shot((p + "83_custom_sounds").c_str());
    std::filesystem::remove(dir + "/first_time.wav");
    std::filesystem::remove(dir + "/alarm.wav");
    app::reloadSounds();
    run(300);
    lines = app::customSounds();
    const bool cleared = lines.empty();
    std::printf("custom sounds: loaded %s, listed %s, removed %s\n", loaded ? "OK" : "FAILED", listed ? "OK" : "FAILED",
                cleared ? "OK" : "FAILED");
    if (!loaded || !listed || !cleared) return 1;
    ui::closeSettings();
    run(300);
  }

  // Hidden aircraft: the eye button on the info card hides one for good (gone
  // now and after later updates, written to hidden.txt); the Hidden aircraft
  // page lists it and brings it back.
  {
    ui::RadarView& rv = ui::RadarView::get();
    rv.goHome(false);
    run(3000);
    std::vector<core::Track> all;
    app::traffic().snapshot(all);
    std::string hex, name;
    for (const core::Track& t : all) {
      if (t.ac.hex[0] && t.ac.hex[0] != '~' && !t.ac.on_ground && t.ac.callsign[0]) {
        hex = t.ac.hex;
        name = t.ac.callsign;
        while (!name.empty() && name.back() == ' ') name.pop_back();
        break;
      }
    }
    auto present = [&] {
      std::vector<core::Track> now;
      app::traffic().snapshot(now);
      for (const core::Track& t : now) {
        if (hex == t.ac.hex) return true;
      }
      return false;
    };
    const bool had = !hex.empty() && present();
    rv.selectAircraft(hex, true);
    run(600);
    if (lv_obj_t* eye = findLabel(lv_screen_active(), LV_SYMBOL_EYE_CLOSE)) {
      lv_obj_send_event(lv_obj_get_parent(eye), LV_EVENT_CLICKED, nullptr);
    }
    run(300);
    const bool asked = hasLabel(lv_layer_top(), ("Hide " + name + "?").c_str());
    shot((p + "84_hide_confirm").c_str());
    if (lv_obj_t* yes = findLabel(lv_layer_top(), LV_SYMBOL_EYE_CLOSE "  Hide")) {
      lv_obj_send_event(lv_obj_get_parent(yes), LV_EVENT_CLICKED, nullptr);
    }
    run(300);
    const bool gone_now = !present() && rv.selected() == nullptr;
    run(12000);  // several feed updates that still carry it
    const bool stays_gone = !present();
    std::string file;
    {
      std::ifstream in(std::string(plat::sdRoot()) + "/hidden.txt");
      file.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    const bool saved = file.find(hex + " " + name) != std::string::npos;
    ui::openSettings(ui::Page::Hidden);
    run(500);
    const bool listed = hasLabel(lv_screen_active(), name.c_str()) && hasLabel(lv_screen_active(), "1 HIDDEN");
    shot((p + "85_hidden_aircraft").c_str());
    if (lv_obj_t* back = findLabel(lv_screen_active(), LV_SYMBOL_EYE_OPEN)) {
      lv_obj_send_event(lv_obj_get_parent(back), LV_EVENT_CLICKED, nullptr);
    }
    run(500);
    const bool emptied = hasLabel(lv_screen_active(), "No hidden aircraft.") && app::hiddenCount() == 0;
    ui::closeSettings();
    run(12000);
    const bool back_again = present();
    std::printf("hidden aircraft (%s): picked %s, asked %s, gone %s, stays gone %s, saved %s, listed %s, "
                "unhidden %s, back %s\n",
                name.c_str(), had ? "OK" : "FAILED", asked ? "OK" : "FAILED", gone_now ? "OK" : "FAILED",
                stays_gone ? "OK" : "FAILED", saved ? "OK" : "FAILED", listed ? "OK" : "FAILED",
                emptied ? "OK" : "FAILED", back_again ? "OK" : "FAILED");
    if (!had || !asked || !gone_now || !stays_gone || !saved || !listed || !emptied || !back_again) return 1;
  }

  // Logbook: a new visitor is announced; the page lists the book.
  {
    auto& st = app::settings();
    st.first_time_alert = true;
    traffic.addVisitor("abcdef", "NEW1ST", "B744");
    bool announced = false;
    for (int i = 0; i < 40 && !announced; ++i) {
      run(250);
      announced = hasLabel(lv_layer_top(), "First time here: NEW1ST") || hasLabel(lv_layer_sys(), "First time here: NEW1ST");
    }
    const app::LogbookSummary book = app::logbookSummary(5);
    const bool booked = book.loaded && !book.learning && book.aircraft > 10;
    std::printf("logbook (%u aircraft): %s, first-time alert %s\n", static_cast<unsigned>(book.aircraft),
                booked ? "OK" : "FAILED", announced ? "OK" : "FAILED");
    if (!booked || !announced) return 1;
    st.first_time_alert = false;
    run(3000);
    ui::openSettings(ui::Page::Logbook);
    run(600);
    shot((p + "82_logbook").c_str());
    ui::closeSettings();
    run(300);
    // The daily log's hours: today's chart has the hours counted so far.
    const core::daylog::Day today = app::dayLogToday();
    int counted = 0;
    for (int h = 0; h < 24; ++h) counted += today.hourly[h];
    ui::openSettings(ui::Page::DayLog);
    run(800);
    const bool chart = counted > 0 && core::daylog::busiestHour(today) >= 0 && hasLabel(lv_screen_active(), "Busiest hour");
    if (lv_obj_t* head = findLabel(lv_screen_active(), "TODAY BY HOUR")) {
      lv_obj_scroll_to_view_recursive(head, LV_ANIM_OFF);
      run(300);
    }
    shot((p + "83_daily_log_hours").c_str());
    ui::closeSettings();
    run(300);
    std::printf("daily log by hour (%d counted): %s\n", counted, chart ? "OK" : "FAILED");
    if (!chart) return 1;
  }

  // Flight replay: what was recorded so far played back, then live again.
  {
    const std::vector<int> days = app::replayDays();
    const bool recorded = !days.empty();
    if (recorded) app::replayStart(days.front());
    run(1500);
    app::ReplayState rs = app::replayState();
    std::vector<core::Track> shown;
    app::traffic().snapshot(shown);
    const bool loaded = rs.active && rs.ready && rs.last_s > rs.first_s && !shown.empty();
    app::replaySeek(rs.first_s);
    app::replaySpeed(60);
    app::replayPlay(true);
    run(3000);
    rs = app::replayState();
    const bool played = rs.pos_s >= rs.first_s + 120 && rs.pos_s <= rs.first_s + 240;
    shot((p + "85_replay").c_str());
    const bool bar = hasLabel(lv_layer_top(), "Replay ") && hasLabel(lv_layer_top(), "60x");
    ui::openSettings(ui::Page::Replay);
    run(400);
    shot((p + "86_replay_days").c_str());
    ui::closeSettings();
    app::replayStop();
    run(3000);
    const bool live = !app::replayActive() && !hasLabel(lv_layer_top(), "Replay ");
    std::printf("replay (%u days): loaded %s, plays %s, bar %s, back to live %s\n", static_cast<unsigned>(days.size()),
                recorded && loaded ? "OK" : "FAILED", played ? "OK" : "FAILED", bar ? "OK" : "FAILED",
                live ? "OK" : "FAILED");
    if (!recorded || !loaded || !played || !bar || !live) return 1;
  }

  // Several Wi-Fi networks: connecting to a new one keeps the old one saved;
  // forgetting one removes it.
  {
    app::useWifi("Cabin", "pines123");
    app::useWifi("Phone hotspot", "");
    std::vector<core::SavedWifi> v = app::savedWifi();
    bool ok = v.size() == 3 && std::strcmp(v[0].ssid, "Phone hotspot") == 0 && std::strcmp(v[1].ssid, "Cabin") == 0 &&
              std::strcmp(v[2].ssid, "HomeNet") == 0 && std::strcmp(app::settings().wifi_ssid, "Phone hotspot") == 0;
    app::useWifi("HomeNet", "hunter2");  // back home: first again, no duplicate
    v = app::savedWifi();
    ok = ok && v.size() == 3 && std::strcmp(v[0].ssid, "HomeNet") == 0 && std::strcmp(v[1].ssid, "Phone hotspot") == 0;
    ui::openSettings(ui::Page::Wifi);
    run(800);
    shot((p + "60_wifi_saved_networks").c_str());
    ui::closeSettings();
    app::forgetWifi("Phone hotspot");
    v = app::savedWifi();
    ok = ok && v.size() == 2 && std::strcmp(v[1].ssid, "Cabin") == 0;
    std::printf("saved wifi networks: %s\n", ok ? "OK" : "FAILED");
    if (!ok) return 1;
    run(500);
  }

  // Partial redraws: after frames that only redrew what changed, the screen
  // must match a full redraw of the same moment, pixel for pixel.
  {
    auto& st = app::settings();
    auto check = [&](const char* what) {
      const std::vector<uint16_t> partial = g_fb;
      lv_obj_invalidate(lv_screen_active());
      lv_refr_now(g_disp);
      size_t diff = 0;
      int fx = -1, fy = -1;
      for (size_t i = 0; i < g_fb.size(); ++i) {
        if (g_fb[i] != partial[i]) {
          if (diff++ == 0) {
            fx = static_cast<int>(i % g_size);
            fy = static_cast<int>(i / g_size);
          }
        }
      }
      // A few pixels may differ where a dirty area's edge cuts through a
      // letter (LVGL 9.4 rounds glyph edges a little differently per area);
      // anything missed for real is far more than that.
      constexpr size_t kGlyphEdgeTolerance = 8;
      const bool ok = diff <= kGlyphEdgeTolerance;
      std::printf("partial redraw (%s): %s", what, ok ? "OK" : "FAILED");
      if (ok && diff) std::printf(" (%zu glyph-edge pixels)\n", diff);
      else if (ok) std::printf("\n");
      if (diff && std::getenv("SIM_DIFF_DUMP")) {
        const std::vector<uint16_t> full = g_fb;
        g_fb = partial;
        writePng(sim::g_out_dir + "/diff_partial.png");
        g_fb = full;
        writePng(sim::g_out_dir + "/diff_full.png");
      }
      if (!ok) {
        std::printf(" - %zu pixels differ, first at %d,%d (%04x partial, %04x full)\n", diff, fx, fy,
                    partial[static_cast<size_t>(fy) * g_size + fx], g_fb[static_cast<size_t>(fy) * g_size + fx]);
      }
      return ok;
    };
    ui::RadarView::get().clearSelection();
    lv_display_trigger_activity(nullptr);
    bool ok = true;
    st.sweep_enabled = true;
    run(3000);
    ok &= check("sweep");
    st.sweep_trail_deg = 0;
    ui::RadarView::get().markStaticDirty();
    run(2000);
    ok &= check("clean bar");
    st.sweep_trail_deg = 70;
    st.sweep_enabled = false;
    run(6000);  // traffic moves on, a fetch arrives
    ok &= check("no sweep");
    st.clock_mode = true;
    st.clock_smooth_seconds = true;
    ui::RadarView::get().markStaticDirty();
    run(2500);
    ok &= check("clock");
    st.clock_mode = false;
    st.clock_smooth_seconds = false;
    st.sweep_enabled = true;
    st.sweep_reveal = true;
    st.show_trails = true;
    ui::RadarView::get().markStaticDirty();
    run(3000);
    ok &= check("echoes fade");
    // The trails fade with their aircraft (drawn on the overlay then), also
    // across a traffic update that moves them on.
    run(6000);
    ok &= check("echoes fade, trails after an update");
    shot((p + "86_echo_trails").c_str());
    st.sweep_reveal = false;
    run(1500);
    ok &= check("trails back on the background");
    int x = 0, y = 0;
    if (findAircraftOnScreen(&x, &y)) {
      tap(x, y);
      run(2000);
      ok &= check("selected aircraft");
      ui::RadarView::get().clearSelection();
    }
    // Resting: aircraft that haven't moved by a whole pixel aren't redrawn,
    // and the screen still matches a full redraw.
    st.sweep_enabled = false;
    ui::RadarView::get().markStaticDirty();
    run(1000);
    const uint32_t f0 = ui::RadarView::get().totals().frames, r0 = ui::RadarView::get().totals().rested;
    run(8000);
    const uint32_t frames = ui::RadarView::get().totals().frames - f0;
    const uint32_t rested = ui::RadarView::get().totals().rested - r0;
    std::printf("resting: %u of %u frames redrew nothing\n", static_cast<unsigned>(rested), static_cast<unsigned>(frames));
    ok &= frames > 0 && check("after resting");
    st.sweep_enabled = true;
    ui::RadarView::get().markStaticDirty();
    run(500);
    if (!ok) return 1;
  }

  // Home Assistant over MQTT: switched on, it connects, announces its sensors,
  // and sends the traffic.
  {
    core::Settings& st = app::settings();
    st.mqtt_enabled = true;
    std::snprintf(st.mqtt_host, sizeof(st.mqtt_host), "%s", "192.168.1.20");
    sim::g_mqtt_sent.clear();
    app::mqtt::service();
    core::LatLon c;
    double r;
    app::queryArea(&c, &r);
    app::ingest(200, g_traffic->json(c, r * 1.25 + 5));
    int online = 0, discovery = 0, state = 0;
    std::string state_payload;
    for (const auto& m : sim::g_mqtt_sent) {
      if (m.topic == "plane_radar/status" && m.payload == "online") ++online;
      if (m.topic.rfind("homeassistant/", 0) == 0 && m.retain) ++discovery;
      if (m.topic == "plane_radar/state" && m.retain) {
        ++state;
        state_payload = m.payload;
      }
    }
    const bool mqtt_ok = online == 1 && discovery == 6 && state == 1 &&
                         state_payload.find("\"count\":") != std::string::npos &&
                         app::mqtt::status() == "Connected to 192.168.1.20";
    std::printf("mqtt: online %d, discovery %d, state %d (%s): %s\n", online, discovery, state,
                state_payload.substr(0, 80).c_str(), mqtt_ok ? "OK" : "FAILED");
    st.mqtt_enabled = false;
    app::mqtt::service();
    if (!mqtt_ok) return 1;
  }

  // "What's new" once after an update (here: from v1.10.0), and again from About.
  {
    ui::RadarView::get().clearSelection();
    std::snprintf(app::settings().last_seen_version, sizeof(app::settings().last_seen_version), "1.10.0");
    run(1500);
    const std::string title = std::string("What's new in v") + plat::firmwareVersion();
    const bool shown = hasLabel(lv_layer_top(), title.c_str());
    shot((p + "78_whats_new").c_str());
    const bool remembered = std::strcmp(app::settings().last_seen_version, plat::firmwareVersion()) == 0;
    lv_obj_clean(lv_layer_top());
    run(1500);
    const bool once = !hasLabel(lv_layer_top(), "What's new in");
    ui::showWhatsNew();
    run(300);
    const bool again = hasLabel(lv_layer_top(), title.c_str());
    lv_obj_clean(lv_layer_top());
    run(300);
    std::printf("what's new: shown %s, once %s, from About %s\n", shown && remembered ? "OK" : "FAILED",
                once ? "OK" : "FAILED", again ? "OK" : "FAILED");
    if (!shown || !remembered || !once || !again) return 1;
  }

  // Settings changed from the phone (or settings.txt): only a new look
  // rebuilds the screens; otherwise the open page just shows the new value.
  {
    ui::openSettings(ui::Page::Sound);
    run(400);
    lv_obj_t* page = lv_screen_active();
    std::vector<std::string> problems;
    app::applySettingsText("volume = 33", &problems);
    run(300);
    const bool kept = lv_screen_active() == page && hasLabel(page, "33%");
    const core::Theme theme = app::settings().theme;
    app::applySettingsText(theme == core::Theme::Amber ? "theme = ocean" : "theme = amber", &problems);
    run(300);
    const bool rebuilt = lv_screen_active() != page;
    std::printf("settings from the phone: value refreshed in place %s, new theme rebuilds %s\n", kept ? "OK" : "FAILED",
                rebuilt ? "OK" : "FAILED");
    // Back as it was, the same way (the page is still open), then close it.
    static const char* const kThemeNames[] = {"phosphor", "amber", "ocean", "night", "custom"};
    app::applySettingsText(std::string("theme = ") + kThemeNames[static_cast<int>(theme)] + "\nvolume = 60", &problems);
    run(300);
    ui::closeSettings(false);
    run(600);
    if (!kept || !rebuilt) return 1;
  }

  // Screenshot button: the picture lands on the card as a PNG.
  {
    ui::RadarView::get().clearSelection();
    lv_display_trigger_activity(nullptr);
    run(600);
    shot((p + "66_camera_button").c_str());
    const size_t before = app::screenshots().size();
    tap(S(673), S(567));
    run(1500);
    const std::vector<std::string> shots = app::screenshots();
    bool ok = shots.size() == before + 1;
    if (ok) {
      const std::string png = readFile(app::screenshotPath(shots[0]));
      core::png::Image img;
      ok = core::png::decode(reinterpret_cast<const uint8_t*>(png.data()), png.size(), &img, g_size * g_size) &&
           img.width == g_size && img.height == g_size;
      std::printf("screenshot %s, %u KB\n", shots[0].c_str(), static_cast<unsigned>(png.size() / 1024));
    }
    std::printf("screenshot button: %s\n", ok ? "OK" : "FAILED");
    if (!ok) return 1;
  }

  // Silencing an aircraft from its info card.
  {
    int x = 0, y = 0;
    bool ok = findAircraftOnScreen(&x, &y);
    if (ok) {
      tap(x, y);
      run(800);
      const core::Track* t = ui::RadarView::get().selected();
      ok = t != nullptr;
      std::string hex = ok ? t->ac.hex : "";
      // The mute button: third from the right in the card's header row (star, mute, follow, close).
      lv_obj_t* hit = nullptr;
      std::function<void(lv_obj_t*)> find = [&](lv_obj_t* o) {
        for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i) {
          lv_obj_t* c = lv_obj_get_child(o, static_cast<int32_t>(i));
          if (lv_obj_check_type(c, &lv_label_class) && std::strcmp(lv_label_get_text(c), LV_SYMBOL_MUTE) == 0 &&
              lv_obj_get_parent(c) != nullptr && lv_obj_is_visible(c)) {
            hit = lv_obj_get_parent(c);
          }
          find(c);
        }
      };
      find(lv_screen_active());
      if (ok && hit != nullptr) {
        lv_area_t a;
        lv_obj_get_coords(hit, &a);
        tap((a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2);
        run(600);
        ok = app::isSilenced(hex.c_str());
        shot((p + "76_card_silenced").c_str());
        ui::openSettings(ui::Page::Silenced);
        run(500);
        shot((p + "77_settings_silenced").c_str());
        ui::closeSettings();
        run(300);
        app::removeSilenced(hex.c_str());
        ok = ok && !app::isSilenced(hex.c_str());
      } else {
        ok = false;
      }
    }
    std::printf("silence from the info card: %s\n", ok ? "OK" : "FAILED");
    if (!ok) return 1;
    ui::RadarView::get().clearSelection();
    run(300);
  }

  // Watchlist: military aircraft near home are announced.
  {
    auto& st = app::settings();
    st.watch_military = true;
    st.watch_radius_km = 400;
    app::traffic().clear();  // seen again: announced
    bool announced = false;
    for (int i = 0; i < 40 && !announced; ++i) {
      run(250);
      announced = hasLabel(lv_layer_top(), "Watchlist: ");
    }
    std::printf("watchlist alert: %s\n", announced ? "OK" : "FAILED");
    if (!announced) return 1;
    run(300);
    shot((p + "67_watchlist_alert").c_str());
    run(4500);
    ui::openSettings(ui::Page::Watchlist);
    run(600);
    shot((p + "68_settings_watchlist").c_str());
    ui::closeSettings();
    st.watch_military = false;
    run(300);
  }

  // Night by the sun: daytime at home now (10 AM in Oregon); night where the
  // sun has set (Sydney, 4 AM there).
  {
    auto& st = app::settings();
    st.night_dim = true;
    st.night_by_sun = true;
    const bool day = !app::isNight();
    const double lat = st.home_lat, lon = st.home_lon;
    st.home_lat = -33.87;
    st.home_lon = 151.21;
    const bool night = app::isNight();
    st.home_lat = lat;
    st.home_lon = lon;
    int set = 0, rise = 0;
    const bool times = app::sunTimesToday(&set, &rise) && set > 16 * 60 && rise < 9 * 60;
    std::printf("night by the sun: day %s, night %s, times %s\n", day ? "OK" : "FAILED", night ? "OK" : "FAILED",
                times ? "OK" : "FAILED");
    if (!day || !night || !times) return 1;
    ui::openSettings(ui::Page::Power);
    run(400);
    lv_obj_scroll_to_y(settingsList(), 99999, LV_ANIM_OFF);
    run(400);
    shot((p + "69_night_by_sun").c_str());
    ui::closeSettings();
    st.night_dim = false;
    st.night_by_sun = false;
    run(300);
  }

  // Phone settings page: the QR code, then requests as a phone would make them.
  {
    ui::openSettings(ui::Page::Phone);
    run(600);
    shot((p + "70_phone_qr").c_str());
    // The radar's own hotspot, for phones that can't reach it through the router.
    const bool hs = app::web::hotspotStart() && plat::hotspotOn() &&
                    app::web::hotspotJoinCode().rfind("WIFI:T:WPA;S:PlaneRadar-", 0) == 0 &&
                    app::web::hotspotAddress() == "http://192.168.4.1:8080";
    std::printf("radar hotspot: %s\n", hs ? "OK" : "FAILED");
    if (!hs) return 1;
    ui::closeSettings();
    run(300);
    ui::openSettings(ui::Page::Phone);
    run(400);
    lv_obj_scroll_to_y(settingsList(), S(600), LV_ANIM_OFF);
    run(400);
    shot((p + "75_phone_hotspot").c_str());
    app::web::hotspotStop();
    ui::closeSettings();
    run(300);
    bool ok = sim::g_web_handler != nullptr && app::web::address() == "http://192.168.1.42:8080";
    plat::WebResponse r = web("GET", "/");
    ok = ok && r.status == 200 && r.body.find("<title>Plane Radar</title>") != std::string::npos;
    const std::string cookie;
    const bool login_ok = ok;
    r = web("GET", "/api/status", "", cookie);
    ok = ok && r.status == 200 && r.body.find("\"board\":\"4in-720x720-chip-v1\"") != std::string::npos;
    r = web("GET", "/api/settings", "", cookie);
    ok = ok && r.body.find("\"watch_types\"") != std::string::npos && r.body.find("hunter") == std::string::npos;
    r = web("POST", "/api/settings", "text=volume%20%3D%2042", cookie);
    ok = ok && r.status == 200 && app::settings().volume == 42;
    r = web("POST", "/api/settings", "text=volume%20%3D%20loud", cookie);
    ok = ok && r.body.find("bad value") != std::string::npos && app::settings().volume == 42;
    const bool settings_ok = ok;
    r = web("POST", "/api/wifi/add", "ssid=Cabin%20Wi-Fi&pass=pines123", cookie);
    ok = ok && r.status == 200 && app::isSavedWifi("Cabin Wi-Fi") &&
         std::strcmp(app::settings().wifi_ssid, "Cabin Wi-Fi") != 0;  // saved, not switched to
    r = web("GET", "/api/wifi", "", cookie);
    ok = ok && r.body.find("\"Cabin Wi-Fi\"") != std::string::npos;
    web("POST", "/api/wifi/forget", "ssid=Cabin%20Wi-Fi", cookie);
    ok = ok && !app::isSavedWifi("Cabin Wi-Fi");
    const bool wifi_ok = ok;
    r = web("GET", "/api/favorites", "", cookie);
    ok = ok && r.status == 200 && r.body.find("\"max\":32") != std::string::npos;
    r = web("GET", "/api/daylog", "", cookie);
    ok = ok && r.body.find("different") != std::string::npos;
    r = web("POST", "/api/screenshot", "", cookie);
    run(1500);
    r = web("GET", "/api/shots", "", cookie);
    const size_t q1 = r.body.find("\"files\":[\"");
    const std::string first = q1 == std::string::npos ? "" : r.body.substr(q1 + 10, r.body.find('"', q1 + 10) - q1 - 10);
    r = web("GET", "/shots/" + first, "", cookie);
    ok = ok && r.status == 200 && r.type == "image/png" && r.body.compare(1, 3, "PNG") == 0;
    ok = ok && web("GET", "/shots/..%2Fsettings.txt", "", cookie).status == 404;
    const bool shots_ok = ok;
    // Backup: download, change one value, restore it; a file that isn't one is refused.
    std::string why;
    r = web("GET", "/api/backup", "", cookie);
    std::string backup = r.body;
    if (!(backup.compare(0, 20, "# Plane Radar backup") == 0 && backup.find("[[favorites]]") != std::string::npos &&
          backup.find("wifi_pass") != std::string::npos)) {
      why += " download";
    }
    const size_t vol = backup.find("\nvolume = ");
    if (vol != std::string::npos) backup.replace(vol + 10, backup.find('\n', vol + 1) - vol - 10, "33");
    r = web("POST", "/api/restore", backup, cookie);
    run(300);
    if (!(r.body.find("\"ok\":true") != std::string::npos && app::settings().volume == 33)) why += " restore(" + r.body + ")";
    r = web("POST", "/api/restore", "not a backup", cookie);
    if (r.body.find("isn't a Plane Radar backup") == std::string::npos) why += " not-a-backup";
    r = web("GET", "/api/logbook", "", cookie);
    if (!(r.body.find("\"loaded\":true") != std::string::npos && r.body.find("\"frequent\":[{") != std::string::npos)) why += " logbook";
    r = web("GET", "/api/logbook.csv", "", cookie);
    if (!(r.status == 200 && r.body.compare(0, 10, "date,time,") == 0 && r.body.find(",first,") != std::string::npos)) why += " logbook.csv";
    r = web("GET", "/api/daylog", "", cookie);
    if (r.body.find("\"hourly\":[") == std::string::npos) why += " daylog-hours";
    r = web("GET", "/api/daylog.csv", "", cookie);
    if (!(r.status == 200 && r.body.compare(0, 14, "date,aircraft,") == 0 && r.body.find("\n2") != std::string::npos)) why += " daylog.csv";
    const bool backup_ok = why.empty();
    if (!backup_ok) std::printf("backup checks failed:%s\n", why.c_str());
    ok = ok && backup_ok;
    std::printf("phone page: page %s, settings %s, wifi %s, screenshots %s, backup, logbook and daily log %s\n", login_ok ? "OK" : "FAILED",
                settings_ok ? "OK" : "FAILED", wifi_ok ? "OK" : "FAILED", shots_ok ? "OK" : "FAILED",
                backup_ok ? "OK" : "FAILED");
    if (!ok) return 1;
    app::settings().volume = 60;
  }

  // Airline and route lookups on the SD card: earlier taps were saved there,
  // and a saved one is answered from the card without asking the internet.
  {
    const std::string sd = plat::sdRoot();
    const bool saved = std::system(("ls -R '" + sd + "/lookups/aircraft' | grep -q '\\.txt'").c_str()) == 0;
    char today[32];
    const std::tm tm = core::localTime(plat::unixTimeMs() / 1000);
    std::snprintf(today, sizeof(today), "%04d%02d%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    if (std::system(("mkdir -p '" + sd + "/lookups/aircraft/ab' '" + sd + "/lookups/routes/TST'").c_str()) != 0) return 1;
    if (FILE* f = std::fopen((sd + "/lookups/aircraft/ab/abc123.txt").c_str(), "wb")) {
      std::fprintf(f, "saved = %s\nmodel = Seen Before 123\nregistration = N-CARD\n", today);
      std::fclose(f);
    }
    if (FILE* f = std::fopen((sd + "/lookups/routes/TST/TST123.txt").c_str(), "wb")) {
      std::fprintf(f, "saved = %s\nairline = Card Airways\n", today);
      std::fclose(f);
    }
    auto server = sim::g_route_server;
    sim::g_route_server = nullptr;  // offline for lookups
    app::requestRoute("abc123", "TST123");
    run(500);  // read from the card (the radar with nothing selected cancels the request after that)
    app::requestRoute("abc123", "TST123");  // now remembered: answered at once
    const app::RouteState rs = app::routeState();
    const bool ok = rs.status == app::RouteStatus::Ready && std::strcmp(rs.info.model, "Seen Before 123") == 0 &&
                    std::strcmp(rs.info.airline, "Card Airways") == 0;
    app::requestRoute("", "");
    sim::g_route_server = server;
    std::printf("lookups saved on the SD card: %s, read back: %s\n", saved ? "OK" : "FAILED", ok ? "OK" : "FAILED");
    if (!saved || !ok) return 1;
    run(300);
  }

  // Firmware update: the radar button is grayed out (up to date), the hourly
  // check finds a newer release and it lights up, then it installs.
  {
    const bool gray = app::updateStatus().state == app::UpdateState::UpToDate;
    std::printf("update auto-check (up to date): %s\n", gray ? "OK" : "FAILED");
    if (!gray) return 1;
    ui::RadarView::get().clearSelection();
    lv_display_trigger_activity(nullptr);  // as if touched: the buttons come back
    run(600);
    shot((p + "57_update_button_gray").c_str());
    // Tapping the grayed-out button checks right away.
    sim::g_update_available = true;
    tap(S(275), S(694));
    run(1500);
    const bool tapped = app::updateStatus().state == app::UpdateState::Available;
    std::printf("update check from the radar button: %s\n", tapped ? "OK" : "FAILED");
    if (!tapped) return 1;
    // The hourly check: back to gray when the release goes away, lit again when it's back.
    sim::g_update_available = false;
    run(3600 * 1000 + 2000, 500);
    const bool regray = app::updateStatus().state == app::UpdateState::UpToDate;
    std::printf("update auto-check (up to date again): %s\n", regray ? "OK" : "FAILED");
    if (!regray) return 1;
    // Automatic checks off: an hour passes and nothing is looked up.
    sim::g_update_available = true;
    app::settings().auto_update_check = false;
    run(3600 * 1000 + 2000, 500);
    const bool quiet = app::updateStatus().state == app::UpdateState::UpToDate;
    std::printf("update auto-check off: %s\n", quiet ? "OK" : "FAILED");
    if (!quiet) return 1;
    app::settings().auto_update_check = true;
    run(3600 * 1000 + 2000, 500);  // an hour later
    const app::UpdateStatus u = app::updateStatus();
    const bool found = u.state == app::UpdateState::Available && std::string(u.latest) == sim::nextVersionTag();
    std::printf("update auto-check (newer release): %s\n", found ? "OK" : "FAILED");
    if (!found) return 1;
    lv_display_trigger_activity(nullptr);  // the buttons are showing
    run(600);
    shot((p + "58_update_button_lit").c_str());
    ui::openSettings(ui::Page::Updates);
    run(600);
    shot((p + "55_update_available").c_str());
    ui::closeSettings();
    run(500);
    ui::confirmInstallUpdate();
    run(300);
    shot((p + "59_update_confirm").c_str());
    app::installUpdate();
    lv_obj_clean(lv_layer_top());  // the confirm dialog
    run(600);
    const bool installed = app::updateStatus().state == app::UpdateState::Installed;
    std::printf("update install: %s\n", installed ? "OK" : "FAILED");
    if (!installed) return 1;
    // While it installs (and until the restart) the radar doesn't draw.
    const uint32_t frames = ui::RadarView::get().totals().frames;
    run(1000);
    const bool paused = ui::RadarView::get().totals().frames == frames && app::updating();
    std::printf("radar paused during the update: %s\n", paused ? "OK" : "FAILED");
    if (!paused) return 1;
    shot((p + "56_update_installed").c_str());
    run(3000);  // restarts (the simulator just logs it)
  }

  std::printf("frames: %d, avg lv_timer_handler %.2f ms (host CPU)\n", g_render_count,
              g_render_ms_total / std::max(1, g_render_count));
  return 0;
}
