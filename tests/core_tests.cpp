// Host unit tests for the platform-independent core (geo, viewport gestures,
// ADS-B parsing, traffic store, traces, sound synthesis). Built by sim/CMakeLists.txt.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <zlib.h>

#include "core/adsb.h"
#include "core/clock.h"
#include "core/filter.h"
#include "core/geo.h"
#include "core/inflate.h"
#include "core/jpeg.h"
#include "core/png.h"
#include "core/rotation.h"
#include "core/route.h"
#include "core/settings_text.h"
#include "core/sound_synth.h"
#include "core/tiles.h"
#include "core/trace.h"
#include "core/update.h"
#include "core/traffic.h"
#include "core/viewport.h"
#include "data/jpeg_fixtures.h"

namespace {

int g_failures = 0;
int g_checks = 0;

#define CHECK(cond)                                                        \
  do {                                                                     \
    ++g_checks;                                                            \
    if (!(cond)) {                                                         \
      ++g_failures;                                                        \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
    }                                                                      \
  } while (0)

#define CHECK_NEAR(a, b, tol)                                                              \
  do {                                                                                     \
    ++g_checks;                                                                            \
    const double _a = (a), _b = (b);                                                       \
    if (std::fabs(_a - _b) > (tol)) {                                                      \
      ++g_failures;                                                                        \
      std::printf("FAIL %s:%d: %s = %.6f, expected %.6f +- %g\n", __FILE__, __LINE__, #a, \
                  _a, _b, static_cast<double>(tol));                                       \
    }                                                                                      \
  } while (0)

void testGeo() {
  const core::LatLon eham{52.3086, 4.7639}, kjfk{40.6398, -73.7789};
  CHECK_NEAR(core::distanceKm(eham, kjfk), 5850, 25);
  CHECK_NEAR(core::bearingDeg({0, 0}, {1, 0}), 0, 1e-6);
  CHECK_NEAR(core::bearingDeg({0, 0}, {0, 1}), 90, 1e-6);
  const core::LatLon d = core::destination(eham, 123.0, 42.0);
  CHECK_NEAR(core::distanceKm(eham, d), 42.0, 1e-6);
  CHECK_NEAR(core::bearingDeg(eham, d), 123.0, 0.01);
  CHECK_NEAR(core::wrapLon(190), -170, 1e-9);
  CHECK_NEAR(core::wrapLon(-181), 179, 1e-9);
  // Destination across the antimeridian stays wrapped.
  const core::LatLon e = core::destination({0, 179.9}, 90, 50);
  CHECK(e.lon < -179.0);
}

void testViewport() {
  core::Viewport vp;
  vp.setScreen(800, 800);
  vp.setCenter({52.0, 5.0});
  vp.setRadiusKm(40);
  CHECK_NEAR(vp.radiusKm(), 40, 1e-9);

  // Center projects to screen center; north is up.
  core::PointF c = vp.project({52.0, 5.0});
  CHECK_NEAR(c.x, 400, 1e-3);
  CHECK_NEAR(c.y, 400, 1e-3);
  core::PointF n = vp.project(core::destination({52.0, 5.0}, 0, 10));
  CHECK_NEAR(n.x, 400, 0.5);
  CHECK_NEAR(n.y, 400 - 10 / vp.kmPerPx(), 0.5);

  // Round trip.
  const core::LatLon g = vp.unproject(123, 654);
  const core::PointF back = vp.project(g);
  CHECK_NEAR(back.x, 123, 1e-3);
  CHECK_NEAR(back.y, 654, 1e-3);

  // Zoom keeps the anchor point fixed.
  const core::LatLon anchor = vp.unproject(600, 250);
  vp.zoomAt(2.0, 600, 250);
  CHECK_NEAR(vp.radiusKm(), 20, 1e-6);
  const core::PointF a2 = vp.project(anchor);
  CHECK_NEAR(a2.x, 600, 0.01);
  CHECK_NEAR(a2.y, 250, 0.01);

  // Dragging right moves the map right: the center moves west.
  const double lon_before = vp.center().lon;
  vp.dragBegin({400, 400}, 0);
  vp.dragUpdate({500, 400}, 16);
  vp.dragEnd(200);  // paused before lift -> no fling
  CHECK(vp.center().lon < lon_before);
  CHECK(!vp.animating());

  // Pinch: fingers spread 2x around a moving midpoint. The map point under the
  // starting midpoint must end up under the final midpoint, at 2x scale.
  vp.setCenter({52.0, 5.0});
  vp.setRadiusKm(40);
  const core::LatLon under = vp.unproject(300, 500);
  vp.pinchBegin({250, 500}, {350, 500});
  vp.pinchUpdate({240, 520}, {360, 520});  // intermediate
  vp.pinchUpdate({250, 560}, {450, 560});  // mid (350,560), distance 200
  vp.pinchEnd();
  CHECK_NEAR(vp.radiusKm(), 20, 1e-6);
  const core::PointF u = vp.project(under);
  CHECK_NEAR(u.x, 350, 0.05);
  CHECK_NEAR(u.y, 560, 0.05);

  // Zoom is clamped.
  vp.zoomAt(1e6, 400, 400);
  CHECK_NEAR(vp.radiusKm(), core::Viewport::kMinRadiusKm, 1e-9);
  vp.zoomAt(1e-6, 400, 400);
  CHECK_NEAR(vp.radiusKm(), core::Viewport::kMaxRadiusKm, 1e-9);

  // flyTo reaches its target exactly.
  vp.flyTo({48.0, 2.0}, 30, 1000, 500);
  CHECK(vp.update(1200));
  vp.update(1600);
  CHECK(!vp.animating());
  CHECK_NEAR(vp.center().lat, 48.0, 1e-9);
  CHECK_NEAR(vp.center().lon, 2.0, 1e-9);
  CHECK_NEAR(vp.radiusKm(), 30, 1e-6);

  // A fast flick keeps gliding, then stops.
  vp.dragBegin({400, 400}, 0);
  for (int i = 1; i <= 5; ++i) vp.dragUpdate({400.0f + i * 20, 400}, i * 10);
  vp.dragEnd(55);
  CHECK(vp.animating());
  uint32_t t = 55;
  while (vp.update(t += 16) && t < 10000) {
  }
  CHECK(t < 10000);

  CHECK_NEAR(core::niceRingStep(40, 4), 10, 1e-9);
  CHECK_NEAR(core::niceRingStep(25, 4), 5, 1e-9);
  CHECK_NEAR(core::niceRingStep(13, 4), 2.5, 1e-9);
  CHECK_NEAR(core::niceRingStep(2, 4), 0.5, 1e-9);
  CHECK_NEAR(core::niceRingStep(250, 4), 50, 1e-9);
}

const char* kSampleJson = R"({
  "ac": [
    {"hex":"484506","flight":"KLM1234 ","r":"PH-BXA","t":"B738","alt_baro":3500,"gs":180.5,
     "track":182.3,"true_heading":180.0,"baro_rate":-768,"squawk":"4521","category":"A3",
     "lat":52.40,"lon":4.80,"seen_pos":2.5},
    {"hex":"3c6444","flight":"DLH4AB  ","t":"A20N","alt_baro":"ground","gs":12,"track":90,
     "lat":52.31,"lon":4.76},
    {"hex":"a1b2c3","flight":"N123AB","alt_geom":12000,"lat":52.5,"lon":5.0,"squawk":"7700"},
    {"hex":"deadbe","flight":"NOPOS"},
    {"hex":"aaaaaa","alt_baro":45000,"lat":52.6,"lon":5.1,"emergency":"general"}
  ],
  "msg":"No error","now":1700000000000,"total":5
})";

void testAdsbParse() {
  std::vector<core::Aircraft> out;
  core::adsb::ParseOptions opt;
  CHECK(core::adsb::parse(kSampleJson, std::strlen(kSampleJson), opt, &out));
  CHECK(out.size() == 3);  // ground and position-less aircraft skipped
  if (out.size() == 3) {
    const core::Aircraft& a = out[0];
    CHECK(std::strcmp(a.callsign, "KLM1234") == 0);  // trailing space trimmed
    CHECK(std::strcmp(a.reg, "PH-BXA") == 0);
    CHECK(std::strcmp(a.type, "B738") == 0);
    CHECK(a.alt_valid && a.alt_ft == 3500);
    CHECK_NEAR(a.track_deg, 182.3, 1e-4);
    CHECK_NEAR(a.heading_deg, 180.0, 1e-4);
    CHECK(a.vrate_fpm == -768);
    CHECK_NEAR(a.seen_pos_s, 2.5, 1e-6);
    CHECK(!a.emergency);
    CHECK(out[1].alt_ft == 12000 && out[1].emergency);  // alt_geom fallback, 7700
    CHECK(out[2].emergency);                           // emergency field
  }

  opt.include_ground = true;
  CHECK(core::adsb::parse(kSampleJson, std::strlen(kSampleJson), opt, &out));
  CHECK(out.size() == 4);
  bool found_ground = false;
  for (const auto& a : out) found_ground |= a.on_ground;
  CHECK(found_ground);

  opt.include_ground = false;
  opt.max_alt_ft = 10000;
  CHECK(core::adsb::parse(kSampleJson, std::strlen(kSampleJson), opt, &out));
  CHECK(out.size() == 1);

  // Military flag (readsb dbFlags bit 0) and the military-only filter.
  const char* mil = R"({"ac":[
    {"hex":"ae1234","flight":"RCH123","t":"C17","dbFlags":1,"alt_baro":24000,"lat":52.1,"lon":5.1},
    {"hex":"ae5678","flight":"DUKE01","t":"H60","dbFlags":3,"alt_baro":1500,"lat":52.2,"lon":5.2},
    {"hex":"484506","flight":"KLM1","t":"B738","dbFlags":2,"alt_baro":9000,"lat":52.3,"lon":5.3},
    {"hex":"484507","flight":"KLM2","t":"B738","alt_baro":9000,"lat":52.4,"lon":5.4}
  ]})";
  core::adsb::ParseOptions mopt;
  CHECK(core::adsb::parse(mil, std::strlen(mil), mopt, &out));
  CHECK(out.size() == 4);
  if (out.size() == 4) CHECK(out[0].military && out[1].military && !out[2].military && !out[3].military);
  mopt.military_only = true;
  CHECK(core::adsb::parse(mil, std::strlen(mil), mopt, &out));
  CHECK(out.size() == 2);
  for (const auto& a : out) CHECK(a.military);

  const char* empty = R"({"ac":null,"msg":"No error"})";
  CHECK(core::adsb::parse(empty, std::strlen(empty), {}, &out));
  CHECK(out.empty());
  const char* broken = R"({"ac":[{"hex":"x")";
  CHECK(!core::adsb::parse(broken, std::strlen(broken), {}, &out));

  CHECK(core::adsb::buildUrl(core::DataSource::AdsbFi, 52.1, 4.2, 18.52) ==
        "https://opendata.adsb.fi/api/v3/lat/52.10000/lon/4.20000/dist/10.0");
  CHECK(core::adsb::buildUrl(core::DataSource::AirplanesLive, 52.1, 4.2, 1000) ==
        "https://api.airplanes.live/v2/point/52.10000/4.20000/250");
  CHECK(core::adsb::buildUrl(core::DataSource::AdsbLol, -33.9, 151.2, 50) ==
        "https://api.adsb.lol/v2/lat/-33.90000/lon/151.20000/dist/27");
}

core::Aircraft mk(const char* hex, double lat, double lon, int alt = 3000) {
  core::Aircraft a;
  std::snprintf(a.hex, sizeof(a.hex), "%s", hex);
  a.lat = lat;
  a.lon = lon;
  a.alt_ft = alt;
  a.alt_valid = true;
  a.gs_kt = 200;
  a.track_deg = 0;
  return a;
}

void testTraffic() {
  core::TrafficStore store;
  core::AlertZone zone{{52.0, 5.0}, 3.0, 5000};

  auto ev = store.update({mk("aaa", 52.5, 5.0), mk("bbb", 52.6, 5.0)}, 1000, zone);
  CHECK(ev.new_aircraft == 0);  // first fetch never chimes
  std::vector<core::Track> snap;
  CHECK(store.snapshot(snap) == 1);
  CHECK(snap.size() == 2);

  ev = store.update({mk("aaa", 52.51, 5.0), mk("bbb", 52.6, 5.0), mk("ccc", 52.0, 5.01)}, 6000, zone);
  CHECK(ev.new_aircraft == 1);
  CHECK(ev.new_close_approaches == 1);  // ccc is ~0.7 km from home at 3000 ft
  ev = store.update({mk("ccc", 52.0, 5.011)}, 11000, zone);
  CHECK(ev.new_close_approaches == 0);  // still inside: alert only once
  ev = store.update({mk("ccc", 52.0, 5.011, 9000)}, 12000, zone);
  ev = store.update({mk("ccc", 52.0, 5.011, 3000)}, 13000, zone);
  CHECK(ev.new_close_approaches == 1);  // left (climbed above) and came back

  store.snapshot(snap);
  const core::Track* aaa = nullptr;
  for (const auto& t : snap) {
    if (std::strcmp(t.ac.hex, "aaa") == 0) aaa = &t;
  }
  CHECK(aaa != nullptr && aaa->trail_count == 2);  // moved ~1.1 km -> two trail points

  // Aircraft missing from fetches are dropped after the timeout.
  store.update({mk("ccc", 52.0, 5.011)}, 6000 + core::TrafficStore::kDropAfterMs + 1, zone);
  store.snapshot(snap);
  CHECK(snap.size() == 1);

  // Trail ring buffer keeps the newest kTrailLen points in order.
  core::TrafficStore s2;
  for (int i = 0; i < core::kTrailLen + 10; ++i) {
    s2.update({mk("ddd", 50.0 + i * 0.01, 5.0)}, 1000 + i * 1000, {});
  }
  s2.snapshot(snap);
  CHECK(snap.size() == 1 && snap[0].trail_count == core::kTrailLen);
  CHECK_NEAR(snap[0].trailAt(0).lat, 50.0 + 10 * 0.01, 1e-4);
  CHECK_NEAR(snap[0].trailAt(core::kTrailLen - 1).lat, 50.0 + (core::kTrailLen + 9) * 0.01, 1e-4);

  // Dead reckoning: 360 kt north for 10 s = 1 nm.
  core::Track t;
  t.ac = mk("eee", 50.0, 5.0);
  t.ac.gs_kt = 360;
  t.fix_ms = 1000;
  const core::LatLon p = core::positionAt(t, 11000);
  CHECK_NEAR(core::distanceKm({50.0, 5.0}, p), core::kKmPerNm, 1e-3);
  CHECK(p.lat > 50.0);
  // seen_pos shifts the fix time back.
  core::TrafficStore s3;
  core::Aircraft old = mk("fff", 50.0, 5.0);
  old.seen_pos_s = 4.0f;
  s3.update({old}, 20000, {});
  s3.snapshot(snap);
  CHECK(snap[0].fix_ms == 16000);
}

void testFavorites() {
  std::vector<core::Favorite> favs(1);
  std::snprintf(favs[0].hex, sizeof(favs[0].hex), "fff111");
  core::FavoriteWatch watch;
  watch.favorites = &favs;
  watch.center = {52.0, 5.0};
  watch.near_radius_km = 10.0;

  core::TrafficStore store;
  core::Aircraft fav = mk("fff111", 52.5, 5.0);  // ~55 km from center
  std::snprintf(fav.callsign, sizeof(fav.callsign), "RCH471");

  // Detected even on the very first fetch; not yet nearby.
  auto ev = store.update({fav, mk("aaa000", 52.6, 5.0)}, 1000, {}, watch);
  CHECK(ev.favorites_seen.size() == 1 && ev.favorites_seen[0] == "RCH471");
  CHECK(ev.favorites_near.empty());

  // Still in range: no repeat "detected".
  ev = store.update({fav}, 6000, {}, watch);
  CHECK(ev.favorites_seen.empty());

  // Enters the nearby radius once.
  fav.lat = 52.05;  // ~5.6 km
  ev = store.update({fav}, 11000, {}, watch);
  CHECK(ev.favorites_near.size() == 1);
  ev = store.update({fav}, 16000, {}, watch);
  CHECK(ev.favorites_near.empty());

  // Leaves and comes back: alerts again.
  fav.lat = 52.3;
  store.update({fav}, 21000, {}, watch);
  fav.lat = 52.02;
  ev = store.update({fav}, 26000, {}, watch);
  CHECK(ev.favorites_near.size() == 1);

  // Drops out of the feed, reappears later: "detected" again.
  store.update({mk("aaa000", 52.6, 5.0)}, 26000 + core::TrafficStore::kDropAfterMs + 1, {}, watch);
  ev = store.update({fav}, 26000 + core::TrafficStore::kDropAfterMs + 5000, {}, watch);
  CHECK(ev.favorites_seen.size() == 1);

  // Non-favorites never trigger favorite events.
  core::TrafficStore other;
  ev = other.update({mk("bbb222", 52.01, 5.0)}, 1000, {}, watch);
  CHECK(ev.favorites_seen.empty() && ev.favorites_near.empty());
  CHECK(core::containsHex(favs, "fff111") && !core::containsHex(favs, "fff112"));
}

void testFilters() {
  CHECK(core::categoryOf("A7") == core::kCatRotor);
  CHECK(core::categoryOf("A5") == core::kCatHeavy && core::categoryOf("A4") == core::kCatHeavy);
  CHECK(core::categoryOf("B4") == core::kCatLight && core::categoryOf("B1") == core::kCatOther);
  CHECK(core::categoryOf("") == core::kCatUnknown && core::categoryOf("A0") == core::kCatUnknown);

  CHECK(core::typeMatches("B738", ""));
  CHECK(core::typeMatches("B738", "b738"));
  CHECK(core::typeMatches("A321", "B738, A32*"));
  CHECK(core::typeMatches("C17", "B738,C17"));
  CHECK(!core::typeMatches("A332", "B738, A32*"));
  CHECK(!core::typeMatches("B7", "B738"));

  core::Settings st;  // defaults: everything airborne shown
  core::Aircraft heli = mk("h", 52, 5, 1500);
  std::snprintf(heli.category, sizeof(heli.category), "A7");
  std::snprintf(heli.type, sizeof(heli.type), "EC35");
  heli.gs_kt = 110;
  core::Aircraft jet = mk("j", 52, 5, 36000);
  std::snprintf(jet.category, sizeof(jet.category), "A3");
  std::snprintf(jet.type, sizeof(jet.type), "A320");
  jet.gs_kt = 460;
  CHECK(core::passesFilters(heli, st) && core::passesFilters(jet, st));
  CHECK(!core::filtersActive(st));

  st.category_mask = core::kCatRotor;  // helicopters only
  CHECK(core::passesFilters(heli, st) && !core::passesFilters(jet, st));
  CHECK(core::filtersActive(st));
  st.category_mask = 0xFF;

  st.min_speed_kt = 200;
  CHECK(!core::passesFilters(heli, st) && core::passesFilters(jet, st));
  st.min_speed_kt = 0;
  st.max_speed_kt = 300;
  CHECK(core::passesFilters(heli, st) && !core::passesFilters(jet, st));
  st.max_speed_kt = core::kSpeedNoLimit;

  st.max_alt_ft = 10000;
  CHECK(core::passesFilters(heli, st) && !core::passesFilters(jet, st));
  st.max_alt_ft = 60000;
  st.min_alt_ft = 5000;
  CHECK(!core::passesFilters(heli, st) && core::passesFilters(jet, st));
  st.min_alt_ft = 0;

  std::snprintf(st.type_filter, sizeof(st.type_filter), "A32*");
  CHECK(!core::passesFilters(heli, st) && core::passesFilters(jet, st));
  st.type_filter[0] = '\0';

  core::Aircraft ground = jet;
  ground.on_ground = true;
  CHECK(!core::passesFilters(ground, st));
  st.show_ground = true;
  CHECK(core::passesFilters(ground, st));
}

void testSounds() {
  for (int i = 0; i < static_cast<int>(core::Sound::kCount); ++i) {
    const auto pcm = core::synthesize(static_cast<core::Sound>(i), 22050);
    CHECK(pcm.size() > 200);
    int peak = 0;
    for (int16_t v : pcm) peak = std::max(peak, std::abs(static_cast<int>(v)));
    CHECK(peak > 3000 && peak <= 32767 * 95 / 100 + 1);
    CHECK(std::abs(pcm.back()) < 200);  // fades out: no click at the end
  }
}

std::string zlibCompress(const std::string& in, int window_bits, int level) {
  z_stream zs{};
  deflateInit2(&zs, level, Z_DEFLATED, window_bits, 8, Z_DEFAULT_STRATEGY);
  std::string out(deflateBound(&zs, in.size()) + 32, '\0');
  zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(in.data()));
  zs.avail_in = static_cast<uInt>(in.size());
  zs.next_out = reinterpret_cast<Bytef*>(&out[0]);
  zs.avail_out = static_cast<uInt>(out.size());
  deflate(&zs, Z_FINISH);
  out.resize(zs.total_out);
  deflateEnd(&zs);
  return out;
}

void testInflate() {
  // Text-like data (dynamic Huffman blocks), random bytes (stored blocks)
  // and a short string (fixed Huffman).
  std::string text;
  for (int i = 0; i < 4000; ++i) text += "[" + std::to_string(i * 7) + ",52.1234,4.5678,\"ground\",null],";
  std::string noise;
  uint32_t x = 12345;
  for (int i = 0; i < 70000; ++i) {
    x = x * 1103515245u + 12345u;
    noise.push_back(static_cast<char>(x >> 24));
  }
  for (const std::string* in : {&text, &noise}) {
    for (int level : {0, 1, 6, 9}) {
      const std::string gz = zlibCompress(*in, 15 + 16, level);
      CHECK(core::isGzip(gz.data(), gz.size()));
      std::string out;
      CHECK(core::gunzip(gz.data(), gz.size(), &out, 1 << 24));
      CHECK(out == *in);
    }
  }
  const std::string small = "hello hello hello";
  const std::string raw = zlibCompress(small, -15, 9);
  std::string out;
  CHECK(core::inflateRaw(raw.data(), raw.size(), &out, 1000) && out == small);

  // Output limit and corrupt input fail cleanly.
  const std::string gz = zlibCompress(text, 15 + 16, 6);
  CHECK(!core::gunzip(gz.data(), gz.size(), &out, 1000));
  std::string bad = gz;
  for (size_t i = 20; i < bad.size(); i += 7) bad[i] = static_cast<char>(bad[i] ^ 0x5A);
  core::gunzip(bad.data(), bad.size(), &out, 1 << 24);  // must not crash; result irrelevant
  CHECK(!core::gunzip(gz.data(), 10, &out, 1 << 24));
  CHECK(!core::isGzip("{}", 2));
}

void testTrace() {
  // Earlier leg (must be dropped), then a new leg from the ground up, with a
  // coverage gap, an aircraft-details object and a point without position.
  const char* json =
      "{\"icao\":\"4ca7b5\",\"r\":\"EI-ABC\",\"t\":\"B738\",\"dbFlags\":0,\"desc\":\"BOEING 737-800\","
      "\"timestamp\": 1700000000.5, \"trace\": ["
      "[0,50.0,4.0,30000,450,90,0,0,null,\"adsb_icao\",30100,0,null,null],"
      "[60,50.0,4.2,30000,450,90,0,0,null,\"adsb_icao\",30100,0,null,null],"
      "[5000,52.3,4.76,\"ground\",12,180,2,null,{\"type\":\"adsb_icao\",\"flight\":\"RYR12  \",\"nav_qnh\":1013.2},\"adsb_icao\",null,null,null,null],"
      "[5060,52.32,4.76,1200,160,10.5,1,1800,null,\"adsb_icao\",1250,1900,170,-2.5],"
      "[5070,null,null,1300,160,10.5,0,1800,null,null,null,null,null,null],"
      "[8000,53.0,5.0,24000,430,45,5,-64,null,\"mlat\",24100,0,300,0.0],"
      "[8030,53.05,5.05,24500,430,45,0,0,null,\"mlat\",null,null,null,null]"
      "]}";
  core::trace::Trace t;
  CHECK(core::trace::parse(json, std::strlen(json), &t));
  CHECK_NEAR(t.timestamp, 1700000000.5, 1e-3);
  CHECK(t.points.size() == 4);
  if (t.points.size() == 4) {
    CHECK((t.points[0].flags & core::trace::kGround) && t.points[0].alt_ft == 0);
    CHECK_NEAR(t.points[0].lat, 52.3, 1e-5);
    CHECK(t.points[1].alt_ft == 1200);
    CHECK(t.points[2].flags & core::trace::kGapBefore);  // ~49 min of silence
    CHECK(!(t.points[3].flags & core::trace::kGapBefore));
    CHECK_NEAR(t.durationS(), 3030, 0.1);
  }

  // Thinning keeps the newest point and stays under the cap.
  std::string big = "{\"timestamp\":0,\"trace\":[";
  for (int i = 0; i < 5000; ++i) {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "%s[%d,%.5f,4.0,%d,300,0,0,0,null]", i ? "," : "", i * 5, 50.0 + i * 0.001,
                  10000);
    big += buf;
  }
  big += "]}";
  CHECK(core::trace::parse(big.data(), big.size(), &t));
  CHECK(t.points.size() <= core::trace::kMaxPoints && t.points.size() > core::trace::kMaxPoints / 2);
  CHECK_NEAR(t.points.back().lat, 50.0 + 4999 * 0.001, 1e-4);

  CHECK(!core::trace::parse("{\"trace\":[]}", 12, &t));
  CHECK(!core::trace::parse("{\"timestamp\":1}", 15, &t));
  CHECK(!core::trace::parse("<html>", 6, &t));
  CHECK(!core::trace::parse(json, std::strlen(json) / 2, &t));  // truncated

  const auto hosts = core::trace::hostsFor(core::DataSource::AdsbLol);
  CHECK(hosts.size() == 3 && hosts[0] == "adsb.lol");
  CHECK(core::trace::buildUrl("globe.adsb.fi", "4ca7b5") ==
        "https://globe.adsb.fi/data/traces/b5/trace_full_4ca7b5.json");
}

void testRoute() {
  // Combined aircraft + flightroute response (shape from adsbdb's README).
  const char* json =
      "{\"response\":{\"aircraft\":{\"type\":\"737-8AS\",\"icao_type\":\"B738\",\"manufacturer\":\"Boeing\","
      "\"mode_s\":\"4CA7B5\",\"registration\":\"EI-DAC\",\"registered_owner_country_iso_name\":\"IE\","
      "\"registered_owner\":\"Ryanair\",\"url_photo\":null},"
      "\"flightroute\":{\"callsign\":\"RYR12AB\",\"callsign_icao\":\"RYR12AB\",\"callsign_iata\":\"FR12AB\","
      "\"airline\":{\"name\":\"Ryanair\",\"icao\":\"RYR\",\"iata\":\"FR\",\"country\":\"Ireland\"},"
      "\"origin\":{\"country_iso_name\":\"IE\",\"elevation\":242,\"iata_code\":\"DUB\",\"icao_code\":\"EIDW\","
      "\"latitude\":53.4,\"longitude\":-6.27,\"municipality\":\"Dublin\",\"name\":\"Dublin Airport\"},"
      "\"destination\":{\"iata_code\":null,\"icao_code\":\"GCRR\",\"municipality\":\"\","
      "\"name\":\"Lanzarote Airport\"}}}}";
  core::route::Info in;
  CHECK(core::route::parse(json, std::strlen(json), &in));
  CHECK(std::strcmp(in.airline, "Ryanair") == 0);
  CHECK(std::strcmp(in.model, "Boeing 737-8AS") == 0);
  CHECK(in.has_route);
  CHECK(std::strcmp(in.origin.code, "DUB") == 0 && std::strcmp(in.origin.city, "Dublin") == 0);
  CHECK(std::strcmp(in.destination.code, "GCRR") == 0);                 // no IATA -> ICAO
  CHECK(std::strcmp(in.destination.city, "Lanzarote Airport") == 0);    // no municipality -> name

  core::route::Info only_ac;
  const char* ac = "{\"response\":{\"aircraft\":{\"type\":\"Cessna 172S\",\"manufacturer\":\"Cessna\","
                   "\"registered_owner\":\"Flying Club\"}}}";
  CHECK(core::route::parse(ac, std::strlen(ac), &only_ac));
  CHECK(std::strcmp(only_ac.model, "Cessna 172S") == 0);  // type already starts with the maker
  CHECK(!only_ac.has_route && std::strcmp(only_ac.owner, "Flying Club") == 0);

  const char* airline = "{\"response\":[{\"name\":\"KLM Royal Dutch Airlines\",\"icao\":\"KLM\"}]}";
  core::route::Info al;
  CHECK(core::route::parse(airline, std::strlen(airline), &al) && std::strcmp(al.airline, "KLM Royal Dutch Airlines") == 0);
  core::route::Info none;
  CHECK(!core::route::parse("{\"response\":\"unknown aircraft\"}", 32, &none));
  CHECK(!core::route::parse("not json", 8, &none));

  CHECK(core::route::airlinePrefix("RYR12AB") == "RYR");
  CHECK(core::route::airlinePrefix("N123AB").empty());
  CHECK(core::route::airlinePrefix("PHABC").empty());
  CHECK(core::route::aircraftUrl("4ca7b5", "RYR12AB ") == "https://api.adsbdb.com/v0/aircraft/4CA7B5?callsign=RYR12AB");
  CHECK(core::route::aircraftUrl("~1234ab", "") == "https://api.adsbdb.com/v0/aircraft/1234AB");
  // Tail-number lookups keep the dash and read back the ICAO address.
  CHECK(core::route::lookupUrl(" ei-dac") == "https://api.adsbdb.com/v0/aircraft/EI-DAC");
  CHECK(std::strcmp(in.mode_s, "4ca7b5") == 0);  // lowercased
  CHECK(std::strcmp(in.registration, "EI-DAC") == 0 && std::strcmp(in.icao_type, "B738") == 0);
}

void testClock() {
  // Time zones with daylight saving. 2026-07-01 12:00 UTC and 2026-01-15 12:00 UTC.
  const int64_t summer = 1782907200, winter = 1768478400;
  int pacific = -1, cet = -1, india = -1;
  for (int i = 0; i < core::timeZoneCount(); ++i) {
    const std::string n = core::timeZone(i).name;
    if (n.rfind("Pacific", 0) == 0) pacific = i;
    if (n.rfind("Central Europe", 0) == 0) cet = i;
    if (n.rfind("India", 0) == 0) india = i;
  }
  CHECK(pacific == 1 && cet > 0 && india > 0);
  core::applyTimeZone(pacific);
  CHECK(core::localTime(summer).tm_hour == 5);   // PDT, UTC-7
  CHECK(core::localTime(winter).tm_hour == 4);   // PST, UTC-8
  core::applyTimeZone(cet);
  CHECK(core::localTime(summer).tm_hour == 14);  // CEST
  CHECK(core::localTime(winter).tm_hour == 13);  // CET
  core::applyTimeZone(india);
  const std::tm ist = core::localTime(winter);
  CHECK(ist.tm_hour == 17 && ist.tm_min == 30);
  core::applyTimeZone(999);  // out of range -> UTC
  CHECK(core::localTime(winter).tm_hour == 12);
  // Every zone's rule parses (a bad rule silently falls back to UTC in newlib/glibc).
  for (int i = 1; i < core::timeZoneCount(); ++i) {
    core::applyTimeZone(i);
    const std::tm a = core::localTime(summer), b = core::localTime(winter);
    CHECK(a.tm_hour != 12 || a.tm_min != 0 || b.tm_hour != 12 || b.tm_min != 0);
  }

  core::Alarm al;
  al.enabled = true;
  al.hour = 7;
  al.minute = 30;
  al.days = 0x3E;  // Mon-Fri
  std::tm t{};
  t.tm_hour = 7;
  t.tm_min = 30;
  t.tm_wday = 2;  // Tuesday
  CHECK(core::alarmMatches(al, t));
  t.tm_wday = 6;  // Saturday
  CHECK(!core::alarmMatches(al, t));
  al.days = 0;    // once: any day
  CHECK(core::alarmMatches(al, t));
  t.tm_min = 31;
  CHECK(!core::alarmMatches(al, t));
  al.enabled = false;
  t.tm_min = 30;
  CHECK(!core::alarmMatches(al, t));

  std::tm c{};
  c.tm_hour = 15;
  c.tm_min = 30;
  c.tm_sec = 15;
  const core::HandAngles h = core::handAngles(c, 500, false);
  CHECK_NEAR(h.hour, 105.0, 0.2);   // 3:30 -> between 3 and 4
  CHECK_NEAR(h.minute, 181.5, 0.01);
  CHECK_NEAR(h.second, 90.0, 0.01);
  CHECK_NEAR(core::handAngles(c, 500, true).second, 93.0, 0.01);

  char buf[40];
  core::formatDays(0x3E, buf, sizeof(buf));
  CHECK(std::strcmp(buf, "Mon-Fri") == 0);
  core::formatDays(0x7F, buf, sizeof(buf));
  CHECK(std::strcmp(buf, "Every day") == 0);
  core::formatDays(0x41, buf, sizeof(buf));
  CHECK(std::strcmp(buf, "Weekends") == 0);
  core::formatDays(0x2A, buf, sizeof(buf));  // Mon Wed Fri
  CHECK(std::strcmp(buf, "Mon Wed Fri") == 0);
  core::formatDays(0, buf, sizeof(buf));
  CHECK(std::strcmp(buf, "Once") == 0);
  core::formatTime(0, 5, false, buf, sizeof(buf));
  CHECK(std::strcmp(buf, "12:05 AM") == 0);
  core::formatTime(12, 0, false, buf, sizeof(buf));
  CHECK(std::strcmp(buf, "12:00 PM") == 0);
  core::formatTime(19, 45, true, buf, sizeof(buf));
  CHECK(std::strcmp(buf, "19:45") == 0);
}

// Minimal PNG writer for the decoder tests: raw packed rows, a chosen filter
// per row (applied here), zlib via compress2, CRCs via zlib's crc32.
std::string pngChunk(const char* type, const std::string& data) {
  std::string out;
  const uint32_t n = static_cast<uint32_t>(data.size());
  for (int i = 3; i >= 0; --i) out.push_back(static_cast<char>(n >> (i * 8)));
  std::string td = std::string(type, 4) + data;
  out += td;
  const uLong crc = crc32(0, reinterpret_cast<const Bytef*>(td.data()), static_cast<uInt>(td.size()));
  for (int i = 3; i >= 0; --i) out.push_back(static_cast<char>(crc >> (i * 8)));
  return out;
}

int paethRef(int a, int b, int c) {
  const int p = a + b - c, pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
  return (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
}

std::string makePng(int w, int h, int color, int depth, const std::vector<std::string>& rows, int bpp,
                    const std::string& plte = "", const std::string& trns = "") {
  std::string raw;
  std::string prev(rows[0].size(), '\0');
  for (int y = 0; y < h; ++y) {
    const int f = y % 5;  // every filter type
    const std::string& r = rows[y];
    std::string fr(r.size(), '\0');
    for (size_t i = 0; i < r.size(); ++i) {
      const int x = static_cast<uint8_t>(r[i]);
      const int a = i >= static_cast<size_t>(bpp) ? static_cast<uint8_t>(r[i - bpp]) : 0;
      const int b = static_cast<uint8_t>(prev[i]);
      const int c = i >= static_cast<size_t>(bpp) ? static_cast<uint8_t>(prev[i - bpp]) : 0;
      int v = x;
      if (f == 1) v = x - a;
      if (f == 2) v = x - b;
      if (f == 3) v = x - ((a + b) >> 1);
      if (f == 4) v = x - paethRef(a, b, c);
      fr[i] = static_cast<char>(v & 0xFF);
    }
    raw.push_back(static_cast<char>(f));
    raw += fr;
    prev = r;
  }
  std::string z(compressBound(raw.size()), '\0');
  uLongf zn = z.size();
  compress2(reinterpret_cast<Bytef*>(&z[0]), &zn, reinterpret_cast<const Bytef*>(raw.data()), raw.size(), 6);
  z.resize(zn);
  std::string ihdr;
  for (uint32_t v : {static_cast<uint32_t>(w), static_cast<uint32_t>(h)}) {
    for (int i = 3; i >= 0; --i) ihdr.push_back(static_cast<char>(v >> (i * 8)));
  }
  ihdr += std::string{static_cast<char>(depth), static_cast<char>(color), 0, 0, 0};
  std::string png = "\x89PNG\r\n\x1a\n";
  png += pngChunk("IHDR", ihdr);
  if (!plte.empty()) png += pngChunk("PLTE", plte);
  if (!trns.empty()) png += pngChunk("tRNS", trns);
  // Split the data over two IDAT chunks, as encoders may.
  png += pngChunk("IDAT", z.substr(0, z.size() / 2));
  png += pngChunk("IDAT", z.substr(z.size() / 2));
  png += pngChunk("IEND", "");
  return png;
}

void testPng() {
  const int w = 13, h = 11;  // odd sizes exercise partial bytes
  auto pix = [](int x, int y, int ch) { return static_cast<uint8_t>((x * 37 + y * 91 + ch * 53) & 0xFF); };
  core::png::Image img;

  // RGBA
  std::vector<std::string> rows;
  for (int y = 0; y < h; ++y) {
    std::string r;
    for (int x = 0; x < w; ++x) {
      for (int c = 0; c < 4; ++c) r.push_back(static_cast<char>(pix(x, y, c)));
    }
    rows.push_back(r);
  }
  std::string png = makePng(w, h, 6, 8, rows, 4);
  CHECK(core::png::decode(reinterpret_cast<const uint8_t*>(png.data()), png.size(), &img));
  int bad = 0;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      for (int c = 0; c < 4; ++c) bad += img.rgba[(y * w + x) * 4 + c] != pix(x, y, c);
    }
  }
  CHECK(img.width == w && img.height == h && bad == 0);

  // RGB
  rows.clear();
  for (int y = 0; y < h; ++y) {
    std::string r;
    for (int x = 0; x < w; ++x) {
      for (int c = 0; c < 3; ++c) r.push_back(static_cast<char>(pix(x, y, c)));
    }
    rows.push_back(r);
  }
  png = makePng(w, h, 2, 8, rows, 3);
  CHECK(core::png::decode(reinterpret_cast<const uint8_t*>(png.data()), png.size(), &img));
  bad = 0;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      for (int c = 0; c < 3; ++c) bad += img.rgba[(y * w + x) * 4 + c] != pix(x, y, c);
      bad += img.rgba[(y * w + x) * 4 + 3] != 255;
    }
  }
  CHECK(bad == 0);

  // Gray and gray+alpha
  for (int color : {0, 4}) {
    rows.clear();
    for (int y = 0; y < h; ++y) {
      std::string r;
      for (int x = 0; x < w; ++x) {
        r.push_back(static_cast<char>(pix(x, y, 0)));
        if (color == 4) r.push_back(static_cast<char>(pix(x, y, 1)));
      }
      rows.push_back(r);
    }
    png = makePng(w, h, color, 8, rows, color == 4 ? 2 : 1);
    CHECK(core::png::decode(reinterpret_cast<const uint8_t*>(png.data()), png.size(), &img));
    bad = 0;
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        const uint8_t* o = &img.rgba[(y * w + x) * 4];
        bad += o[0] != pix(x, y, 0) || o[2] != pix(x, y, 0) || o[3] != (color == 4 ? pix(x, y, 1) : 255);
      }
    }
    CHECK(bad == 0);
  }

  // Palette at 8, 4, 2 and 1 bits, with transparency for entry 0.
  for (int depth : {8, 4, 2, 1}) {
    const int colors = 1 << (depth > 4 ? 5 : depth);
    std::string plte;
    for (int i = 0; i < colors; ++i) {
      plte.push_back(static_cast<char>(i * 7));
      plte.push_back(static_cast<char>(255 - i * 5));
      plte.push_back(static_cast<char>(i * 3 + 1));
    }
    auto idx = [&](int x, int y) { return (x * 3 + y * 5) % colors; };
    rows.clear();
    for (int y = 0; y < h; ++y) {
      std::string r((w * depth + 7) / 8, '\0');
      for (int x = 0; x < w; ++x) {
        const int per = 8 / depth;
        const int shift = (per - 1 - x % per) * depth;
        r[x / per] = static_cast<char>(static_cast<uint8_t>(r[x / per]) | (idx(x, y) << shift));
      }
      rows.push_back(r);
    }
    png = makePng(w, h, 3, depth, rows, 1, plte, std::string(1, '\x40'));
    CHECK(core::png::decode(reinterpret_cast<const uint8_t*>(png.data()), png.size(), &img));
    bad = 0;
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        const uint8_t* o = &img.rgba[(y * w + x) * 4];
        const int i = idx(x, y);
        bad += o[0] != static_cast<uint8_t>(i * 7) || o[1] != static_cast<uint8_t>(255 - i * 5) ||
               o[3] != (i == 0 ? 0x40 : 255);
      }
    }
    CHECK(bad == 0);
  }

  // Rejections: truncated data, not a PNG, too large.
  CHECK(!core::png::decode(reinterpret_cast<const uint8_t*>(png.data()), png.size() / 2, &img));
  CHECK(!core::png::decode(reinterpret_cast<const uint8_t*>("GIF89a....."), 11, &img));
  CHECK(!core::png::decode(reinterpret_cast<const uint8_t*>(png.data()), png.size(), &img, 100));
}

void testJpeg() {
  core::png::Image img;
  CHECK(core::jpeg::isJpeg(kJpegBaseline, sizeof(kJpegBaseline)));
  CHECK(core::jpeg::decode(kJpegBaseline, sizeof(kJpegBaseline), &img));
  CHECK(img.width == 32 && img.height == 24);
  int worst = 0;
  for (int y = 0; y < 24 && img.width == 32; ++y) {
    for (int x = 0; x < 32; ++x) {
      const uint8_t* o = &img.rgba[(y * 32 + x) * 4];
      const int want[3] = {x * 8, y * 10, ((x / 8 + y / 8) % 2) ? 128 : 40};
      for (int c = 0; c < 3; ++c) worst = std::max(worst, std::abs(o[c] - want[c]));
      worst = std::max(worst, 255 - o[3] == 0 ? 0 : 999);
    }
  }
  CHECK(worst <= 24);  // lossy, and sharp checker edges ring a little
  CHECK(!core::jpeg::decode(kJpegProgressive, sizeof(kJpegProgressive), &img));  // unsupported: fails cleanly
  CHECK(!core::jpeg::decode(kJpegBaseline, sizeof(kJpegBaseline) / 3, &img));    // truncated
  CHECK(!core::jpeg::decode(kJpegBaseline, sizeof(kJpegBaseline), &img, 100));   // too large
}

void testTiles() {
  using namespace core::tiles;
  // Redmond, OR at zoom 10: tile 167/371 (values from the standard slippy-map formula).
  CHECK(tileX(-121.15, 10) == 167);
  CHECK(tileY(44.254, 10) == 371);
  CHECK_NEAR(tileLon(167, 10), -121.289, 0.01);
  CHECK(tileLat(371, 10) >= 44.254 && tileLat(372, 10) <= 44.254);
  CHECK_NEAR(tileLat(0, 1), 85.0511, 1e-3);
  CHECK_NEAR(tileLat(1, 1), 0.0, 1e-9);

  // ~1 screen px per tile px: at 45 deg, zoom 10 tiles have ~108 m pixels.
  CHECK(zoomFor(Variant::DarkLabels, 0.108, 45.0) == 10);
  CHECK(zoomFor(Variant::DarkLabels, 0.216, 45.0) == 9);
  CHECK(zoomFor(Variant::Topo, 0.0001, 45.0) == 17);   // capped
  CHECK(zoomFor(Variant::DarkLabels, 500.0, 45.0) == 1);

  const uint64_t k = key(Variant::Topo, 12, 669, 1485);
  Variant v;
  int z;
  uint32_t x, y;
  unpack(k, &v, &z, &x, &y);
  CHECK(v == Variant::Topo && z == 12 && x == 669 && y == 1485);
  unpack(parentKey(k, 2), &v, &z, &x, &y);
  CHECK(z == 10 && x == 167 && y == 371);
  CHECK(key(Variant::DarkLabels, 12, 669, 1485) != k);

  CHECK(url(Variant::DarkLabels, 3, 1, 2) == "https://a.basemaps.cartocdn.com/dark_all/3/1/2.png");
  CHECK(url(Variant::DarkNoLabels, 3, 1, 2, 1) == "https://a.basemaps.cartocdn.com/rastertiles/dark_nolabels/3/1/2.png");
  CHECK(url(Variant::Topo, 5, 6, 7) == "https://a.tile.opentopomap.org/5/6/7.png");
  CHECK(sdPath("/sdcard", Variant::Topo, 5, 6, 7) == "/sdcard/tiles/topo/5/6/7.png");
  CHECK(variantFor(Style::DarkStreets, false, true) == Variant::DarkNoLabels);
  CHECK(variantFor(Style::DarkStreets, true, false) == Variant::OsmStandard);  // no CARTO key
  CHECK(variantFor(Style::Off, true, true) == Variant::None);
  CHECK(variantFor(Style::FaaSectional, true, false) == Variant::FaaSectional);
  CHECK(url(Variant::FaaSectional, 9, 82, 183) ==
        "https://tiles.arcgis.com/tiles/ssFJjBXIUyZDrSYZ/arcgis/rest/services/VFR_Sectional/MapServer/tile/9/183/82");
  CHECK(zoomFor(Variant::FaaSectional, 2.0, 45.0) == 8);     // wide view: coarsest zoom it has
  CHECK(zoomFor(Variant::FaaSectional, 0.001, 45.0) == 12);  // close: enlarged
  CHECK(keepColors(Variant::FaaSectional) && !keepColors(Variant::Topo));
  CHECK(url(Variant::DarkLabels, 3, 1, 2, 0, "abc123") ==
        "https://a.basemaps.cartocdn.com/dark_all/3/1/2.png?key=abc123");
  CHECK(url(Variant::OsmStandard, 3, 1, 2) == "https://tile.openstreetmap.org/3/1/2.png");
  CHECK(isLight(Variant::OsmStandard) && !isLight(Variant::DarkLabels));
  // Keyed CARTO tiles don't share folders with the watermarked ones saved before.
  CHECK(sdPath("/sdcard", Variant::DarkLabels, 1, 0, 0) == "/sdcard/tiles/cdark/1/0/0.png");
}

void testMapRecolor() {
  // Pixels: black, white, mid gray, transparent white.
  const uint8_t rgba[] = {0, 0, 0, 255, 255, 255, 255, 255, 128, 128, 128, 255, 255, 255, 255, 0};
  uint16_t out[4];
  core::tiles::recolor(rgba, 4, false, true, 0, 255, 0, 100, out);
  CHECK(out[0] == 0);              // dark map: black stays black
  CHECK(out[1] == (63 << 5));      // white -> full tint (green)
  CHECK((out[2] >> 11) == 0 && ((out[2] >> 5) & 63) > 20 && ((out[2] >> 5) & 63) < 63);
  CHECK(out[3] == 0);              // transparent -> black
  core::tiles::recolor(rgba, 4, true, true, 0, 255, 0, 100, out);  // light map: inverted
  CHECK(out[0] == (63 << 5) && out[1] == 0);
  core::tiles::recolor(rgba, 4, false, false, 0, 255, 0, 50, out);
  CHECK(out[1] == ((127 >> 3) << 11 | (127 >> 2) << 5 | (127 >> 3)));  // original colors at half brightness
}

void testRotation() {
  const int W = 720, H = 720;
  for (int q = 0; q < 4; ++q) {
    for (int i = 0; i < 50; ++i) {
      const int ux = (i * 37) % W, uy = (i * 91) % H;
      int px, py, bx, by;
      core::uiToPanel(ux, uy, q, W, H, &px, &py);
      CHECK(px >= 0 && px < W && py >= 0 && py < H);
      core::panelToUi(px, py, q, W, H, &bx, &by);
      CHECK(bx == ux && by == uy);
    }
  }
  // 90 deg clockwise: the UI's top-left corner sits at the panel's top-right.
  int px, py;
  core::uiToPanel(0, 0, 1, W, H, &px, &py);
  CHECK(px == W - 1 && py == 0);
  core::uiToPanel(0, 0, 2, W, H, &px, &py);
  CHECK(px == W - 1 && py == H - 1);
  core::uiToPanel(0, 0, 3, W, H, &px, &py);
  CHECK(px == 0 && py == H - 1);

  // Pixels of a stripe land where uiToPanel() puts their coordinates, so the
  // picture and touch always agree. Odd sizes and a padded source stride.
  const int sx = 13, sy = 41, sw = 37, sh = 9, stride = 40;
  std::vector<uint16_t> src(stride * sh), dst(sw * sh);
  for (int y = 0; y < sh; ++y) {
    for (int x = 0; x < sw; ++x) src[y * stride + x] = static_cast<uint16_t>(y * 1000 + x);
  }
  for (int q = 0; q < 4; ++q) {
    core::rotatePixels(src.data(), sw, sh, stride, dst.data(), q);
    const core::Rect r = core::uiRectToPanel({sx, sy, sx + sw - 1, sy + sh - 1}, q, W, H);
    const int dw = r.x2 - r.x1 + 1;
    CHECK(dw == ((q & 1) ? sh : sw) && r.y2 - r.y1 + 1 == ((q & 1) ? sw : sh));
    int bad = 0;
    for (int y = 0; y < sh; ++y) {
      for (int x = 0; x < sw; ++x) {
        int ppx, ppy;
        core::uiToPanel(sx + x, sy + y, q, W, H, &ppx, &ppy);
        if (dst[(ppy - r.y1) * dw + (ppx - r.x1)] != src[y * stride + x]) ++bad;
      }
    }
    CHECK(bad == 0);
  }
}

}  // namespace

void testSettingsText() {
  namespace st = core::settings_text;
  // Round trip: a changed Settings written and read back into defaults matches.
  core::Settings a;
  std::snprintf(a.wifi_ssid, sizeof(a.wifi_ssid), "My Net #2");
  std::snprintf(a.wifi_pass, sizeof(a.wifi_pass), " p=ss \"word\" ");
  a.home_lat = 47.4502;
  a.home_lon = -122.3088;
  a.home_set = true;
  a.units = core::Units::Nm;
  a.theme = core::Theme::Custom;
  a.rotation = 3;
  a.source = core::DataSource::AdsbLol;
  a.category_mask = core::kCatAirliner | core::kCatHeavy;
  a.alarms[0] = {true, 6, 45, 0x3E};
  a.alarms[2] = {true, 22, 5, 0x22};  // Mon, Fri
  a.map_style = 3;
  a.time_zone = 13;
  std::snprintf(a.carto_key, sizeof(a.carto_key), "abc123");
  std::snprintf(a.type_filter, sizeof(a.type_filter), "B738, A32*");
  const std::string text = st::serialize(a);
  core::Settings b;
  std::vector<std::string> problems;
  const int n = st::parse(text, &b, &problems);
  CHECK(problems.empty());
  for (const std::string& p : problems) std::printf("  %s\n", p.c_str());
  CHECK(n > 80);
  CHECK(std::strcmp(b.wifi_ssid, a.wifi_ssid) == 0);
  CHECK(std::strcmp(b.wifi_pass, a.wifi_pass) == 0);
  CHECK_NEAR(b.home_lat, 47.4502, 1e-9);
  CHECK_NEAR(b.home_lon, -122.3088, 1e-9);
  CHECK(b.home_set && b.units == core::Units::Nm && b.theme == core::Theme::Custom);
  CHECK(b.rotation == 3 && b.source == core::DataSource::AdsbLol && b.map_style == 3 && b.time_zone == 13);
  CHECK(b.category_mask == (core::kCatAirliner | core::kCatHeavy));
  CHECK(b.alarms[0].enabled && b.alarms[0].hour == 6 && b.alarms[0].minute == 45 && b.alarms[0].days == 0x3E);
  CHECK(b.alarms[2].days == 0x22 && !b.alarms[1].enabled);
  CHECK(std::strcmp(b.carto_key, "abc123") == 0 && std::strcmp(b.type_filter, "B738, A32*") == 0);
  CHECK(st::serialize(b) == text);  // stable
  CHECK(text.find("rotation = 270") != std::string::npos);
  CHECK(text.find("alarm3 = on 22:05 mon,fri") != std::string::npos);
  CHECK(text.find("categories = airliner, heavy") != std::string::npos);

  // Hand-edited file: partial, odd spacing, CRLF, comments, clamping, problems.
  core::Settings c;
  c.brightness = 33;
  problems.clear();
  const int m = st::parse(
      "# hi\r\n[Display]\r\n  UNITS=KM \r\nsweep_rpm = 500\r\nrotation = 90\r\nshow_rings = false\r\n"
      "source = airplaneslive\nalarm1 = on 7:30 mon-wed\nalarm2 = on 25:00\nbogus = 1\nwifi_ssid = Home\n"
      "max_alt_ft = 1000\nmin_alt_ft = 5000\ncategories = all\njunk line\n",
      &c, &problems);
  CHECK(m == 10);
  CHECK(problems.size() == 3);  // alarm2, bogus, junk line
  CHECK(c.units == core::Units::Km && c.sweep_rpm == 60 && c.rotation == 1 && !c.show_rings);
  CHECK(c.source == core::DataSource::AirplanesLive && c.brightness == 33);
  CHECK(c.alarms[0].enabled && c.alarms[0].hour == 7 && c.alarms[0].minute == 30 && c.alarms[0].days == 0x0E);
  CHECK(!c.alarms[1].enabled);
  CHECK(std::strcmp(c.wifi_ssid, "Home") == 0);
  CHECK(c.min_alt_ft == 1000 && c.max_alt_ft == 5000);  // swapped into order
  CHECK(c.category_mask == 0xFF);
  // Too-long strings keep the old value.
  core::Settings d;
  st::parse("wifi_ssid = 0123456789012345678901234567890123456789\n", &d, nullptr);
  CHECK(d.wifi_ssid[0] == '\0');

  // Favorites file.
  std::vector<core::Favorite> favs;
  std::vector<std::string> lookups;
  st::parseFavorites(
      "# comment\nA1B2C3 N123AB b738\n\n  ad4f21   # just hex\nn456cd\nA1B2C3 dup\nC-GABC   # Canadian\nN456CD\n",
      &favs, &lookups);
  CHECK(favs.size() == 2);
  CHECK(std::strcmp(favs[0].hex, "a1b2c3") == 0 && std::strcmp(favs[0].label, "N123AB") == 0 &&
        std::strcmp(favs[0].type, "B738") == 0);
  CHECK(std::strcmp(favs[1].hex, "ad4f21") == 0 && std::strcmp(favs[1].label, "AD4F21") == 0);
  CHECK(lookups.size() == 2 && lookups[0] == "N456CD" && lookups[1] == "C-GABC");
  std::vector<core::Favorite> favs2;
  st::parseFavorites(st::serializeFavorites(favs), &favs2, &lookups);
  CHECK(favs2.size() == 2 && lookups.empty());
  CHECK(std::strcmp(favs2[0].type, "B738") == 0 && std::strcmp(favs2[1].hex, "ad4f21") == 0);
  CHECK(st::isIcaoHex("abcdef") && !st::isIcaoHex("abcdeg") && !st::isIcaoHex("abcde") && !st::isIcaoHex("abcdef0"));
}

void testUpdate() {
  namespace up = core::update;
  int v[3];
  CHECK(up::parseVersion("v1.12.1", v) && v[0] == 1 && v[1] == 12 && v[2] == 1);
  CHECK(up::parseVersion("2.0.10", v) && v[2] == 10);
  CHECK(up::parseVersion("1.2.3-beta", v));
  CHECK(!up::parseVersion("1.2", v) && !up::parseVersion("abc", v) && !up::parseVersion("1.2.3x", v));
  CHECK(up::compareVersions("v1.12.1", "1.12.1") == 0);
  CHECK(up::compareVersions("1.9.0", "v1.12.0") < 0);  // numeric, not text
  CHECK(up::compareVersions("v2.0.0", "1.99.99") > 0);
  CHECK(up::compareVersions("junk", "1.0.0") < 0);
  CHECK(up::assetName("4in-720x720-chip-v1", "v1.13.0") == "plane-radar-4in-720x720-chip-v1-v1.13.0-ota.bin");

  const std::string json = R"({"tag_name":"v1.13.0","name":"v1.13.0","assets":[
    {"name":"plane-radar-4in-720x720-chip-v1-v1.13.0.bin","size":6092304,
     "browser_download_url":"https://github.com/x/releases/download/v1.13.0/usb.bin"},
    {"name":"plane-radar-4in-720x720-chip-v3-v1.13.0-ota.bin","size":5999000,
     "browser_download_url":"https://github.com/x/releases/download/v1.13.0/v3-ota.bin"},
    {"name":"plane-radar-4in-720x720-chip-v1-v1.13.0-ota.bin","size":5990000,
     "browser_download_url":"https://github.com/x/releases/download/v1.13.0/v1-ota.bin"}]})";
  up::Release r;
  CHECK(up::parseLatestRelease(json.data(), json.size(), "4in-720x720-chip-v1", &r));
  CHECK(r.tag == "v1.13.0" && r.asset_url == "https://github.com/x/releases/download/v1.13.0/v1-ota.bin" &&
        r.asset_size == 5990000);
  // A release without an update image for this board (e.g. one from before updates over Wi-Fi).
  CHECK(up::parseLatestRelease(json.data(), json.size(), "3.4in-800x800-chip-v1", &r));
  CHECK(r.tag == "v1.13.0" && r.asset_url.empty());
  CHECK(!up::parseLatestRelease("{\"message\":\"rate limited\"}", 26, "x", &r));
  CHECK(!up::parseLatestRelease("not json", 8, "x", &r));
}

int main() {
  testGeo();
  testViewport();
  testAdsbParse();
  testTraffic();
  testFavorites();
  testFilters();
  testInflate();
  testTrace();
  testRotation();
  testPng();
  testTiles();
  testJpeg();
  testMapRecolor();
  testClock();
  testRoute();
  testSettingsText();
  testUpdate();
  testSounds();
  std::printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
