<p align="center">
  <img src="docs/images/hero.jpg" alt="Plane Radar P4: live ADS-B air traffic on a round touchscreen">
</p>

<p align="center">
  <a href="https://github.com/ShadowStacker/P4-Radar/releases/latest"><img alt="Latest release" src="https://img.shields.io/github/v/release/ShadowStacker/P4-Radar?style=flat-square&color=3dff95&labelColor=0b1f14"></a>
  <a href="https://github.com/ShadowStacker/P4-Radar/actions/workflows/build.yml"><img alt="Build" src="https://img.shields.io/github/actions/workflow/status/ShadowStacker/P4-Radar/build.yml?style=flat-square&labelColor=0b1f14"></a>
  <a href="https://github.com/ShadowStacker/P4-Radar/releases"><img alt="Downloads" src="https://img.shields.io/github/downloads/ShadowStacker/P4-Radar/total?style=flat-square&color=3dff95&labelColor=0b1f14"></a>
  <img alt="ESP32-P4" src="https://img.shields.io/badge/ESP32--P4-ESP--IDF%205.5-3dff95?style=flat-square&labelColor=0b1f14">
  <img alt="LVGL 9.6" src="https://img.shields.io/badge/LVGL-9.6-3dff95?style=flat-square&labelColor=0b1f14">
</p>

<p align="center">
  <b><a href="#-get-started-in-5-minutes">Get started</a></b> ·
  <b><a href="docs/USER_GUIDE.md">User guide</a></b> ·
  <b><a href="#-a-tour">Tour</a></b> ·
  <b><a href="docs/AIRCRAFT_ICONS.md">Aircraft icons</a></b> ·
  <b><a href="CHANGELOG.md">Changelog</a></b> ·
  <b><a href="https://github.com/ShadowStacker/P4-Radar/releases">Downloads</a></b>
</p>

---

**Plane Radar P4** turns a Waveshare ESP32-P4 round touch display into a live
air-traffic radar for your desk, wall or window sill. It shows every aircraft
around you on a real map, with each one drawn in its own shape, rotated to its
heading and coloured by altitude. Tap one to see where it's going.

It needs no computer, app or account: Wi-Fi, a home location and a few seconds
are all it takes. It's also much more than a radar:

<table>
<tr>
<td width="33%" valign="top"><h3>✈️ See everything</h3><b>470 aircraft icons</b> covering about 2,900 type codes, from a Cub to an A380, gliders, gyroplanes and balloons. Trails, one-minute speed vectors, 48,000 airports with runways, and smooth motion between updates.</td>
<td width="33%" valign="top"><h3>🗺️ On a real map</h3>Dark streets, <b>topo</b>, <b>FAA sectional charts</b>, <b>satellite</b> or a street map with names, tinted to the theme. Tiles are saved on an SD card, so a map you've seen also works offline.</td>
<td width="33%" valign="top"><h3>🔔 Never miss one</h3><b>Favorites</b>, a <b>watchlist</b> (military, helicopters, fast jets, any type or callsign), proximity and emergency alerts, and <i>first time here</i>. Each has its own sound, and you can use your own WAV files.</td>
</tr>
<tr>
<td width="33%" valign="top"><h3>📒 Remember everything</h3>A <b>daily log</b> with hourly charts, a <b>logbook</b> of every aircraft ever seen near home, and <b>a week of replay</b>: scrub through any recorded day of traffic.</td>
<td width="33%" valign="top"><h3>🏠 Fits your home</h3>A <b>clock face</b> with four alarms, night dimming or sleep (by the clock or sunset to sunrise), five themes, and <b>Home Assistant</b> sensors and events over MQTT.</td>
<td width="33%" valign="top"><h3>📡 Your data, your way</h3>Three free community feeds with automatic failover, or <b>your own receiver</b> (readsb, tar1090, dump1090, SkyAware). Settings are editable from a <b>phone page</b>, and updates install <b>over Wi-Fi</b>.</td>
</tr>
</table>

<p align="center">
  <img src="docs/images/sweep.gif" width="300" alt="The radar sweep">
  <img src="docs/images/04_aircraft_selected.png" width="300" alt="An aircraft's info card">
  <img src="docs/images/35_flight_history.png" width="300" alt="Flight history">
</p>

---

## 🚀 Get started in 5 minutes

**You need:** a [Waveshare ESP32-P4-WIFI6-Touch-LCD-3.4C](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-3.4c.htm)
(3.4", 800×800) or **-4C** (4", 720×720), a USB-C cable, and optionally a
microSD card (FAT32) and a small speaker.

1. **Download** the firmware for your board from the
   [latest release](https://github.com/ShadowStacker/P4-Radar/releases/latest):

   | Board | File |
   |---|---|
   | 3.4" round, 800×800 | `plane-radar-3.4in-800x800-chip-v1-<version>.bin` |
   | 4" round, 720×720 | `plane-radar-4in-720x720-chip-v1-<version>.bin` |

   > Use the file **without** `-ota` in its name. The `-ota.bin` files are for
   > updates over Wi-Fi.

2. **Flash it at address `0x0`**, either from your browser with
   [esptool-js](https://espressif.github.io/esptool-js/) (Chrome or Edge;
   change the address from `0x1000` to **`0x0`**), or from a terminal:

   ```bash
   pip install --upgrade esptool
   python -m esptool --chip esp32p4 -b 460800 write_flash 0x0 plane-radar-4in-720x720-chip-v1-<version>.bin
   ```

3. **Follow the screen.** Pick your Wi-Fi and type the password on the
   on-screen keyboard. Then set your home: drag a map, type an airport code
   like `KJFK`, or search for your town. Traffic appears within seconds.

From then on the radar **updates itself**: a button lights up when a new
version is out. Step-by-step help is in
[Flashing the firmware](docs/USER_GUIDE.md#flashing-the-firmware).

<p align="center">
  <img src="docs/images/01_first_boot_wifi.png" width="250" alt="Wi-Fi setup">
  <img src="docs/images/02_wifi_password.png" width="250" alt="On-screen keyboard">
  <img src="docs/images/17_pick_home.png" width="250" alt="Picking home on the map">
</p>

---

## 🧭 A tour

### The radar

North is up and home is in the middle, with range rings in miles, kilometres or
nautical miles. Each aircraft's label shows its callsign, its altitude (feet,
or flight level up high), a climb or descent arrow and its type. Drag to pan,
pinch to zoom from 1 km to 460 km, and double-tap to zoom in. Long-press any
spot to make it home or bookmark it.

<p align="center">
  <img src="docs/images/03_radar_home.png" width="270" alt="The radar">
  <img src="docs/images/33_central_oregon_30km.png" width="270" alt="Airports and runways">
  <img src="docs/images/18_emergency.png" width="270" alt="An emergency squawk">
</p>

Tap an aircraft for its **info card**: route (*Dublin > Lanzarote*), airline
and exact model, altitude, vertical speed, speed, track, distance from home and
squawk. Its **whole flight so far** is drawn behind it. The card's buttons
**★ save it as a favourite**, **mute** it, **hide** it for good, or **follow** it
across the map.

### Maps

<p align="center"><img src="docs/images/strip_maps.jpg" alt="The five background maps"></p>

Choose a map from the picture button on the radar. It loads the area you're
looking at, the closest tiles first, and keeps them on the SD card for
next time.

### Themes and looks

<p align="center"><img src="docs/images/strip_themes.jpg" alt="Green, amber, blue, night and custom themes"></p>

Green phosphor, amber, blue, a dim red **night** theme, or your own colours. You
can also set:

- **Aircraft colours:** by altitude (orange near the ground → green → blue →
  magenta up high) or the theme colour;
- **Size:** of the aircraft symbols and of the labels;
- **The sweep:** a clean bar or a long smooth trail, at any speed, or a
  real-scope **echo** mode where aircraft fade until the beam passes them again.

### Alerts that matter

| | |
|---|---|
| ⭐ **Favorites** | Up to 32 aircraft, by tail number or from their card. A gold ring, an always-on label, and a fanfare when one shows up and again when it comes near. |
| 🎯 **Watchlist** | Announces military aircraft, helicopters, fast jets, or any types (`A388, C17, B52`) and callsigns (`RCH*`) within a distance of home. |
| 🆕 **First time here** | The logbook remembers every aircraft that ever came by; a new one gets its own announcement. |
| 🚨 **Emergencies** | Squawk 7500/7600/7700 pulses red and sounds a siren. |
| 📍 **Proximity** | Anything low and close to home. |
| 🔇 **Silence / hide** | Mute a noisy local trainer, or hide an aircraft completely. |

<p align="center">
  <img src="docs/images/24_favorite_on_radar.png" width="270" alt="A favorite on the radar">
  <img src="docs/images/67_watchlist_alert.png" width="270" alt="A watchlist alert">
  <img src="docs/images/68_settings_watchlist.png" width="270" alt="Watchlist settings">
</p>

### Logs, logbook and replay

With an SD card in, the radar keeps:

- **a daily log:** how many aircraft flew by, the busiest hour, the closest
  pass, the highest and fastest, with charts and 7- and 30-day totals;
- **a logbook:** every aircraft ever seen near home, in a spreadsheet file;
- **a week of recorded traffic** to replay at up to 300× speed.

<p align="center">
  <img src="docs/images/64_daily_log.png" width="250" alt="Daily log">
  <img src="docs/images/82_logbook.png" width="250" alt="Logbook">
  <img src="docs/images/85_replay.png" width="250" alt="Replaying a day">
</p>

### Clock, alarms and night

Tap the clock button and the compass becomes a clock face, with the radar still
running under the hands. You can set four alarms with snooze. At night the
screen can dim, or sleep and wake by itself, by the clock or from sunset to
sunrise at your home.

<p align="center">
  <img src="docs/images/39_clock_face.png" width="250" alt="Clock face">
  <img src="docs/images/43_alarm_ringing.png" width="250" alt="An alarm">
  <img src="docs/images/69_night_by_sun.png" width="250" alt="Night by the sun">
</p>

### Your phone as a remote

Scan the QR code on the radar and change any setting from your phone. From the
same page you can:

- add favourites by tail number;
- manage Wi-Fi networks;
- read the daily log;
- download screenshots and backups;
- install a firmware file.

<p align="center">
  <img src="docs/images/70_phone_qr.png" width="230" alt="Phone page QR code">
  <img src="docs/images/phone_settings.png" width="170" alt="Phone page: settings">
  <img src="docs/images/phone_watch.png" width="170" alt="Phone page: watchlist">
  <img src="docs/images/phone_log.png" width="170" alt="Phone page: daily log">
</p>

### Your own receiver and Home Assistant

Running a Pi with readsb, tar1090, dump1090 or a FlightAware/FR24 feeder?
Choose **My receiver** and type its address. Traffic then comes straight from
your antenna, in real time, with no internet needed for it.

Turn on **Home Assistant (MQTT)** and a *Plane Radar* device appears in Home
Assistant by itself. It has these entities:

- aircraft nearby;
- the nearest aircraft, its distance and altitude;
- an emergency sensor;
- an alert event to trigger automations, such as *flash the porch light when
  a favourite flies over*.

<p align="center">
  <img src="docs/images/12_settings_traffic_receiver.png" width="270" alt="Your own receiver">
  <img src="docs/images/86_settings_home_assistant.png" width="270" alt="Home Assistant">
  <img src="docs/images/87_nearby_aircraft.png" width="270" alt="Nearby aircraft">
</p>

### Built to keep running

- **Safe updates:** updates over Wi-Fi are checked before install. A version
  that fails to start rolls back by itself, and three quick power cycles go
  back to the previous version.
- **Automatic recovery:** a feed that stops answering is replaced by another,
  and lost Wi-Fi is rejoined.
- **Settings that survive:** they're also kept on the SD card in an editable
  `settings.txt`, and you can download one backup file with everything.
- **Diagnostics:** the About page has memory graphs for the last 24 hours, task
  stack use and restart reasons, and on-screen stats show live performance.

<p align="center">
  <img src="docs/images/58_update_button_lit.png" width="250" alt="Update available">
  <img src="docs/images/13_settings_about_memory.png" width="250" alt="Diagnostics">
  <img src="docs/images/65_stats_overlay.png" width="250" alt="On-screen stats">
</p>

**Every screen, menu and setting is explained in the [User guide](docs/USER_GUIDE.md).**

---

## 🔩 Hardware

| Part | On the board |
|---|---|
| **SoC** | ESP32-P4: dual-core RISC-V at 360 MHz, 32 MB PSRAM (the firmware uses 16 MB of flash) |
| **Display** | 3.4" 800×800 or 4" 720×720 round IPS, MIPI-DSI |
| **Touch** | GT911 capacitive multi-touch |
| **Wi-Fi** | ESP32-C6 co-processor (Wi-Fi 6, 2.4 GHz) over SDIO, via ESP-Hosted |
| **Audio** | ES8311 codec and amplifier: connect a small speaker to the speaker connector |
| **Storage** | microSD slot (optional, FAT32): map tiles, logs, replays, settings backup |

The pin mapping comes from Waveshare's BSP, so there's nothing to wire. Both
boards run the same firmware build with their own screen size.

<p align="center">
  <img src="docs/images/720_radar_home.png" width="250" alt="The 4-inch 720x720 board">
  <img src="docs/images/720_aircraft_selected.png" width="250" alt="The 4-inch board, info card">
  <br><sub>The 4" 720×720 board: the same interface, scaled to fit.</sub>
</p>

---

## 🛠️ Building from source

You need **ESP-IDF v5.5.2 or newer** (CI uses v5.5.6). Every component is
pinned to an exact version in `main/idf_component.yml`.

```bash
# 3.4" board, ESP32-P4 chip revision v1.x (what these boards have)
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.p4rev1" set-target esp32p4
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.p4rev1" build flash monitor

# 4" board
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.4inch;sdkconfig.defaults.p4rev1" build flash monitor
```

<details>
<summary><b>Chip revisions, releases and CI</b></summary>

- **Chip revisions.** ESP32-P4 chips come in two incompatible families,
  **v3.x** and **v0.x/v1.x**: an image built for one won't boot on the other.
  The Waveshare boards have v1.x chips, so releases are built with the
  `sdkconfig.defaults.p4rev1` overlay (`chip-v1` in the file name). Without the
  overlay the build targets v3.x.
- **Changed defaults.** If you change `sdkconfig.defaults`, delete `sdkconfig`
  so the new defaults take effect.
- **CI.** GitHub Actions (`.github/workflows/build.yml`) builds both boards,
  runs the unit tests and renders the simulator screenshots on every push.
- **Releasing.**
  1. Add a `## [vX.Y.Z]` section to [CHANGELOG.md](CHANGELOG.md) and bump
     `CONFIG_APP_PROJECT_VER` in `sdkconfig.defaults`.
  2. Push a `vX.Y.Z` tag, or run the **Build** workflow with *release_version*
     set.

  CI publishes the USB images, the `-ota.bin` update files, `SHA256SUMS.txt`
  and `symbols-*.elf` files for decoding crash backtraces. The release notes
  are that version's changelog section, which is also the radar's *What's new*
  screen.

</details>

### Desktop simulator and tests

The UI reaches the hardware only through `main/platform/platform.h`, so the
same code runs on a PC. Every screenshot in this README and the guide comes
from it.

```bash
cmake -S sim -B build-sim && cmake --build build-sim -j
./build-sim/core_tests                        # ~2,700 checks: geo math, parsing, alerts, logbook, MQTT...
./build-sim/plane_radar_sim sim_out           # scripted taps, drags and pinches -> PNG screenshots + WAV sounds
./build-sim/plane_radar_sim sim_out --size 720
./build-sim/plane_radar_sim --bench out       # timing with 450 aircraft
./build-sim/plane_radar_sim --web out         # serves the phone page on http://localhost:8080
SIM_REAL_TILES=1 ./build-sim/plane_radar_sim sim_out   # real map tiles (cached) instead of stand-ins
```

LVGL and cJSON are fetched automatically (`-DLVGL_DIR=…` / `-DCJSON_DIR=…`
build offline). The UI compiles against LVGL 9.2 through 9.6.

<details>
<summary><b>Project layout and how rendering works</b></summary>

```
main/
  main.cpp              boot sequence
  core/                 pure C++, no LVGL or ESP-IDF: geo math, viewport (pan, pinch, fling, fly-to),
                        traffic store, alerts, ADS-B parser, flight history, logbook, daily log,
                        replay, settings text, sound synthesis, MQTT messages, aircraft icon table
  app/                  the app: network loop, feeds and failover, SD card jobs, updates,
                        phone web server, Home Assistant bridge
  ui/                   LVGL UI: radar_view (cached background + per-frame overlay), hud,
                        settings_ui, keyboard, sprites, theme
  platform/platform.h   the hardware interface used by app and ui
  platform/esp/         ESP-IDF implementation: display and touch, Wi-Fi, HTTPS, OTA, NVS, audio, MQTT
  data/                 built-in airports and runways, aircraft icon masks
scripts/                generators: airports, aircraft icons and their type table, icon docs
sim/                    desktop simulator (headless LVGL, fake traffic, PNG/WAV output, bench)
tests/                  host unit tests
```

The software renderer draws 800×800 at up to 30 fps.

- **Base layer:** map, rings, compass, runways and airport codes, kept in a
  PSRAM canvas and redrawn only when the view changes.
- **Trails:** drawn on top of a copy of the base when new traffic arrives, with
  nearly straight stretches merged into single lines.
- **Every frame:** the sweep, aircraft and labels are drawn over that.
- **Aircraft icons:** rotated to 72 headings on first use, kept in a PSRAM cache
  of at most 4 MB, and tinted to the aircraft's colour as they're drawn.

</details>

---

## 🙏 Data and credits

- **Aircraft icons by ADS-B Radar for macOS - https://adsb-radar.com -
  https://apps.apple.com/app/id1538149835.** The free icon set is used with the
  backlink its licence asks for. The rest of the icons are drawn in the same
  style from each aircraft's real dimensions (`scripts/aircraft_catalog.py`).
- **Aircraft positions** come from the free community feeds
  [adsb.fi](https://adsb.fi), [airplanes.live](https://airplanes.live) and
  [adsb.lol](https://adsb.lol). Please keep the update interval reasonable
  (5 s by default).
- **Airline, route and airframe details** come from
  [adsbdb.com](https://www.adsbdb.com) (route data by David Taylor and Jim
  Mason, aircraft data from PlaneBase).
- **Maps:**
  - [CARTO](https://carto.com/basemaps) Dark Matter, [OpenStreetMap](https://www.openstreetmap.org/copyright)
    and [OpenTopoMap](https://opentopomap.org) (OpenStreetMap data, SRTM);
  - Esri World Imagery;
  - the FAA's VFR Sectional chart tiles. **Not for navigation.**
- **Place search:** [Nominatim](https://nominatim.org) (OpenStreetMap).
- **Airports and runways:** [OurAirports](https://ourairports.com/data/) (public
  domain).
- **JPEG decoding:** [TJpgDec](http://elm-chan.org/fsw/tjpgd/) by ChaN.
- **Inspiration:** [MatixYo/ESP32-Plane-Radar](https://github.com/MatixYo/ESP32-Plane-Radar)
  (MIT).
