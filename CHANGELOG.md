# Changelog

All notable changes to Plane Radar P4. Versions match the
[GitHub releases](https://github.com/ShadowStacker/P4-Radar/releases).

**Which file do I flash?** Each release has four firmware images. Pick the one
for your board size **and** your ESP32-P4 chip revision. See
[Flashing](docs/USER_GUIDE.md#flashing-the-firmware) in the user guide.

Upgrading keeps your Wi-Fi, home location, settings and favorites, unless an
entry below says otherwise.

## [v1.6.0] - 2026-10-27

### Added
- **Clock face**: a new clock button on the radar (right side) swaps the
  compass headings for clock numerals and minute marks, with hour, minute and
  seconds hands and a date window. The radar keeps running underneath.
- **Settings → Clock & alarms**:
  - Time zone, from a list of 24 with automatic daylight saving (default
    US Pacific).
  - 24-hour time and seconds-hand options.
  - **Four alarms**, each with a time, the days it repeats (or once), and a
    test button.
- **Alarm screen** with **Snooze 9 min** and **Stop**. Alarms wake a dimmed
  screen, beep at no less than 70 % volume even when sound is off, and give
  up after 10 minutes.
- The time is set from the internet (NTP) once Wi-Fi connects.

## [v1.5.1] - 2026-10-27

### Fixed
- **Freeze with the screen rotated** (90°, 180° or 270°), for example when
  opening Display settings. The display driver's built-in rotation hung on
  busy screens. The firmware now rotates the picture itself and runs the
  driver unrotated, the same way it runs at 0°. Your rotation setting is
  kept, so after flashing the board starts up rotated and no longer freezes.

## [v1.5.0] - 2026-10-27

### Added
- **Airline and route on the info card**: tap an aircraft to see where it's
  flying from and to (*Dublin (DUB) > Lanzarote (ACE)*), the airline (or the
  owner for private aircraft) and the exact model (*Boeing 737-8AS*). Looked
  up on adsbdb.com and remembered for the last 24 aircraft. Routes come from
  a community list by flight number, so now and then one is out of date.
  Switch: Display → **Airline and route on tap**.
- **Custom theme** (Display → Theme → Custom, then **Custom colors**): sliders
  for the radar color, background brightness, ring/compass brightness,
  aircraft color and label color, with live swatches and a reset button.
- **Aircraft size** (60–160 %) and **Label size** (Small / Normal / Large) in
  Display, for every theme.
- **Filter button** on the radar (right side) opens Filters directly, and is
  lit while a filter is hiding aircraft.
- **Mute button** on the radar (left side) turns all sounds on or off.
- **Airport code style** (Display): ICAO or IATA codes on the map.

### Changed
- **X on a settings page now discards your changes** since you opened
  settings and goes back to the radar. **<** keeps them. Wi-Fi changes and
  confirmed restarts can't be undone this way.
- The **status pill** (aircraft count and zoom) now fades out with the
  buttons. It stays visible while it's showing a problem, such as being
  offline.
- **Distance units default to miles.** Boards upgrading from older versions
  switch to miles once; you can change it back in Display.
- Airports with only a local code show their IATA code instead, for example
  Prineville shows **PRZ** instead of S39. Proper ICAO codes like KRDM stay.

## [v1.4.0] - 2026-10-27

### Added
- **Flight history**: tap an aircraft and its whole current flight is
  downloaded and drawn behind it as a thicker line, colored by altitude, with
  a ring where it took off. Zoom out to see the route.
  - The info card shows the status: *loading history...*, *history 1h 42m*,
    or *no history*.
  - Only the current leg is drawn (earlier flights that day are left out),
    at most 14 hours of it. It refreshes every 2 minutes while selected.
  - Comes from the selected feed's globe map, with the other two feeds as
    backups. These files aren't an official API, so if a feed stops serving
    them the card says *no history* and everything else keeps working.
  - Switch: Display → **Flight history on tap** (on by default).
- **Screen rotation** (Display → Screen rotation): 0°, 90°, 180° or 270°
  clockwise, for boards mounted another way round. Touch rotates with the
  picture. It's applied on restart, and the radar offers to restart right
  away.

### Notes
- Rotation is done by the display driver while copying each frame, which
  costs a little speed at 90°, 180° and 270°. If animation feels slow
  rotated, shorten the sweep trail. (v1.5.1 replaced this with the
  firmware's own rotation after it froze on some screens.)

### Documentation
- New [user guide](docs/USER_GUIDE.md) covering every screen, gesture and
  setting, with defaults, plus how alerts work, flashing and troubleshooting.
- This changelog. Release notes now start with the version's changelog
  section.
- README: quick start, documentation links, updated screenshots.

## [v1.3.0] - 2026-10-27

### Added
- **Filters page** (Settings → Filters) for choosing which aircraft are shown:
  - Aircraft kinds: light aircraft, small & business, airliners, heavies
    (757/widebody), fast jets, helicopters, gliders/balloons/drones, and
    aircraft with no category. Each has its own switch.
  - Type codes with wildcards, e.g. `B738, A32*, C17`.
  - Altitude band and ground-speed band.
  - Military only and ground traffic (moved here from Traffic & data).
  - **Reset all filters**.
- Favorites are never hidden by filters, so their alerts always work.
- The status pill shows how many aircraft the filters are hiding, e.g.
  "3 aircraft (11 filtered)".
- **Sweep trail** slider (Display → Animation): 0 gives a clean bar with no
  trail, up to 180°. The default is 70°, as before.
- **Smooth trail fade** option: a finer, softer gradient with no banding.
- About **43,000 small airports**, for roughly 48,000 airports in total. In
  Central Oregon that adds Bend (KBDN), Prineville (S39), Madras (KS33),
  Sunriver (KS21), Sisters (K6K5) and local strips.
  - Small fields appear below about 35 km of zoom.
  - Unpaved runways, and strips without surveyed coordinates (drawn on their
    runway-number heading through the airport), are drawn thinner and fainter.
- "Use an airport" accepts FAA codes, and US codes without the K (`RDM`,
  `BDN`, `S39`).

### Changed
- The Back and Close buttons on settings pages moved inward so the round
  display doesn't clip them.
- Traffic & data now only holds the feed and update interval, plus a link to
  Filters.

### Notes
- The firmware image is about 5.8 MB, up from 2.6 MB, because of the airport
  data.

## [v1.2.0] - 2026-10-27

### Added
- **Favorites**: tap the star on an aircraft's info card to save it (up to
  32).
  - Saved aircraft get a gold ring, a star badge and an always-on gold label.
    When off-screen they show as a gold dot on the rim.
  - Matching uses the airframe's ICAO address, so a favorite is still
    recognized under a different flight number.
- **Settings → Favorites** page: lists saved aircraft, whether each is in
  range and how far away, with a remove button.
- **Favorite detected** alert: a fanfare when a saved aircraft comes into
  range.
- **Favorite nearby** alert: a two-tone chime when a saved aircraft comes
  within an adjustable **Nearby radius** of home (1–100, default 15 km).
- Both favorite alerts also show an on-screen notice, even with sound off.
- Test buttons for the new sounds.

### Changed
- The **proximity alert** (any aircraft inside the alert radius) has its own
  switch and is **off by default**.
- The old "Alerts & emergencies" switch is now **Emergency squawks**.
- **Auto-hide buttons** is now on by default. Boards upgrading from older
  versions switch to the new default once.

## [v1.1.0] - 2026-10-27

### Added
- **Military aircraft only** filter. Military aircraft are identified from the
  feed's aircraft database.
- A **MIL** badge on the info card of military aircraft.
- **Auto-hide buttons after 10 s** (Display): the radar's round buttons fade
  out when idle, and any touch brings them back.

### Fixed
- Upgrading firmware no longer resets settings to defaults when new settings
  are added.

## [v1.0.3] - 2026-10-27

### Fixed
- **Crash and reboot right after "Loading..."**. The aircraft images used up
  the chip's internal RAM; they now live in PSRAM. First firmware confirmed
  working on real hardware.

### Added
- A free-memory report in the boot log.

## [v1.0.2] - 2026-10-27

### Added
- Releases include `symbols-*.elf` files, so crash backtraces can be decoded.
  No firmware changes.

## [v1.0.1] - 2026-10-27

### Fixed
- **Boot loop on current boards (chip revision v3.x)**: black screen and USB
  reconnecting over and over. The firmware is now built with ESP-IDF v5.5.6,
  and releases ship separate images for v3.x and v1.x chips.
- The board no longer reboots in a loop if the Wi-Fi co-processor (ESP32-C6)
  doesn't respond. The radar runs offline and says so.

### Added
- A boot screen showing each startup step.
- CI checks each image's chip-revision range.

## [v1.0.0] - 2026-10-27

First release.

- ADS-B radar for the Waveshare ESP32-P4-WIFI6-Touch-LCD-3.4C (800×800) and
  -4C (720×720), using adsb.fi, airplanes.live or adsb.lol.
- Touch controls: pan, pinch-zoom, double-tap zoom, Home button, aircraft info
  card with Follow, and long-press to set home.
- Radar sweep animation and synthesized sound effects.
- Full touch settings: Wi-Fi with on-screen keyboard, location, display,
  sound, traffic and about.
- Known issue: only boots on ESP32-P4 chip revision v1.x (fixed in v1.0.1).

[v1.6.0]: https://github.com/ShadowStacker/P4-Radar/releases/tag/v1.6.0
[v1.5.1]: https://github.com/ShadowStacker/P4-Radar/releases/tag/v1.5.1
[v1.5.0]: https://github.com/ShadowStacker/P4-Radar/releases/tag/v1.5.0
[v1.4.0]: https://github.com/ShadowStacker/P4-Radar/releases/tag/v1.4.0
[v1.3.0]: https://github.com/ShadowStacker/P4-Radar/releases/tag/v1.3.0
[v1.2.0]: https://github.com/ShadowStacker/P4-Radar/releases/tag/v1.2.0
[v1.1.0]: https://github.com/ShadowStacker/P4-Radar/releases/tag/v1.1.0
[v1.0.3]: https://github.com/ShadowStacker/P4-Radar/releases/tag/v1.0.3
[v1.0.2]: https://github.com/ShadowStacker/P4-Radar/releases/tag/v1.0.2
[v1.0.1]: https://github.com/ShadowStacker/P4-Radar/releases/tag/v1.0.1
[v1.0.0]: https://github.com/ShadowStacker/P4-Radar/releases/tag/v1.0.0
