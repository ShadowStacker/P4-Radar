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
#include "core/rotation.h"
#include "core/route.h"
#include "core/sound_synth.h"
#include "core/trace.h"
#include "core/traffic.h"
#include "core/viewport.h"

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
  testClock();
  testRoute();
  testSounds();
  std::printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
