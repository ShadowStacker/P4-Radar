// Host unit tests for the platform-independent core (geo, viewport gestures,
// ADS-B parsing, traffic store, sound synthesis). Built by sim/CMakeLists.txt.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "core/adsb.h"
#include "core/geo.h"
#include "core/sound_synth.h"
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

}  // namespace

int main() {
  testGeo();
  testViewport();
  testAdsbParse();
  testTraffic();
  testSounds();
  std::printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
