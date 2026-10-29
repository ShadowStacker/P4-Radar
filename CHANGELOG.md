# Changelog

All notable changes to Plane Radar P4. Versions match the
[GitHub releases](https://github.com/ShadowStacker/P4-Radar/releases).

**Which file do I flash?** Each release has four firmware images. Pick the one
for your board size **and** your ESP32-P4 chip revision. See
[Flashing](docs/USER_GUIDE.md#flashing-the-firmware) in the user guide.

From v1.12.0 on, upgrading keeps your Wi-Fi, home location, settings and
favorites. **Before v1.12.0 every update erased them** (see v1.12.0).

## [v1.18.3] - 2026-10-29

### Added
- **The radar's own hotspot** for the phone page (Settings → Phone settings
  page → Start the radar's hotspot), for when the phone can't reach the radar
  through the router. One QR code joins the phone to the radar's Wi-Fi, a
  second one opens the page. The radar stays on your Wi-Fi meanwhile, and the
  hotspot turns off by itself after 30 minutes without a phone.

 - 2026-10-29

### Fixed
- **The phone settings page didn't open.** It's now on port 50080 (the QR
  code and the address on the radar include it). The radar's Wi-Fi chip can
  keep incoming connections on port 80 for itself; ports 49152-61439 always
  reach the radar.

### Removed
- The PIN: the page opens directly. Anyone on your network can use it; it
  isn't reachable from the internet, and it can be turned off on the radar
  (Settings → Phone settings page).
- `plane-radar.local`: with the Wi-Fi chip keeping network announcements for
  itself it can't work reliably. Use the address shown on the radar.

 - 2026-10-29

### Fixed
- Wi-Fi power saving is off: asleep between beacons, the radio could be slow
  to notice (or miss) a phone opening the settings page.

### Added
- The radar's Phone settings page says whether the page is working: **Server**
  (the radar asks its own page) and **Phone visits** (requests that reached the
  radar, and from where), to tell a network problem from a radar problem.
  Every request is also in the serial log (`web: GET / from ...`).

 - 2026-10-29

### Added
- **Phone settings page**: Settings → Phone settings page shows a QR code;
  scan it and a page served by the radar opens on your phone, already logged
  in (or open `http://plane-radar.local` and enter the PIN). Change every
  setting, add and rename favorites, manage saved Wi-Fi networks (scan, add
  for later or connect now, reorder, forget), see the daily log and
  screenshots, and install a firmware file. The radar refuses the full USB
  image and files for another board. PIN protected, local network only.
- **Watchlist alerts** (Settings → Watchlist alerts): a notice and a sound
  when military aircraft, helicopters, fast jets, chosen types (A388, C17,
  ...) or callsigns and tail numbers (RCH*, N123AB) come within a distance of
  home. Watched aircraft are always shown, whatever the filters.
- **Night from sunset to sunrise** (Display → Night is): night mode can follow
  the sun at your home location instead of fixed times.
- **Screenshot button** on the radar (camera, below the clock): saves the
  screen as a PNG in `screenshots/` on the SD card.
- **Lookups kept on the SD card**: airline, route and airframe details from
  adsbdb.com are saved in `lookups/`, so aircraft seen before show them at
  once, also offline (SD card page: switch and delete button).

### Changed
- The zoom, map, Home and update buttons moved a little closer together to
  make room for the camera button.

## [v1.17.1] - 2026-10-29

### Added
- **Sleep at night** (Display → Night mode → At night: Sleep): instead of
  dimming, the radar goes to sleep (screen off, sounds muted) when the night
  starts and wakes by itself when it ends. A tap wakes it during the night;
  after a minute without a touch it goes back to sleep.

### Fixed
- A brief stutter every few seconds: when new traffic arrived, the whole
  background layer (rings, runways, trails, labels) was redrawn in one frame.
  It is now redrawn off-screen a quarter at a time and swapped in when done.
  The map credit line is also no longer rewritten twice a second.

### Removed
- The weather radar and aircraft photos, which didn't work reliably. Their
  saved settings are ignored and the rest of your settings are kept.

## [v1.17.0] - 2026-10-29

### Added
- **Weather radar** over any map (map picker, or Display → Weather radar):
  precipitation from RainViewer, looping the last hour by default, with
  adjustable opacity. The credit line shows the time of the picture.
- **Aircraft photos** on the info card, from planespotters.net, with the
  photographer's name (Display → Aircraft photo on tap).
- **Daily log** (Settings → Daily log): different aircraft near home today,
  the busiest moment, the closest pass, the highest and fastest aircraft,
  military and emergency counts, favorites seen. Saved per day on the SD card
  as `log/YYYY-MM-DD.txt`, with the last week listed.
- **Night dimming** by the clock (Display → Dim at night): a lower brightness
  between two times, e.g. 10 PM to 7 AM.
- **Diagnostics** in About: uptime, why it last restarted, how many starts
  and how many were unexpected (crashes, watchdogs, power dips).
- **On-screen stats** (About → On-screen stats, off by default): frame rate,
  draw time, CPU per core, memory, aircraft, Wi-Fi signal, uptime and the
  longest screen pause, live in the lower left.
- **Emergency rollback:** power-cycling three times in a row, each within
  20 seconds of starting, goes back to the previously installed version.
- **Automatic feed failover:** after three failed updates in a row the radar
  switches to the next ADS-B source, and goes back to the chosen one once it
  answers again (checked every 10 minutes).

### Changed
- The aircraft data connection stays open between updates (no new TLS
  handshake every few seconds), and map tiles, weather and the feed each keep
  their own connection.
- The serial log no longer prints "Certificate validated" for every
  connection.
- Fresh README screenshots, including all the newer screens.

## [v1.16.1] - 2026-10-29

### Fixed
- **More of the cyan flashes** (at startup, and now and then even in the
  menus). The panel shows a cyan frame whenever its DMA can't fetch the next
  pixels in time. Three changes keep it fed:
  - The panel refreshes at about 66 Hz instead of 131 (pixel clock 40 MHz
    instead of the board package's 80). That halves the memory traffic and
    doubles the gap between frames.
  - The interrupt that restarts the DMA after every frame runs on core 1 with
    the UI, away from the Wi-Fi, SD card and encryption work on core 0.
  - It also runs at a higher priority (3 instead of 1).
- **Display menu freezing.** Every change was saved to flash 2.5 s later, and
  writing flash pauses both cores, sometimes for a few hundred milliseconds.
  In the settings menu, changes are now saved when you leave it (and at least
  once a minute).

## [v1.16.0] - 2026-10-29

### Added
- **Two new background maps** (map button, or Display → Background map):
  - **Satellite**: aerial and satellite photos (Esri World Imagery).
  - **Street map**: OpenStreetMap in its own colors, with street names once
    you zoom in to a few miles.
- **Settings → Updates**, a page of its own: installed and latest version,
  Check now, Install, and **Check for updates automatically** (on by default;
  turn it off to stop the hourly checks). The update rows moved here from
  About.

### Changed
- **Smoother, finer zoom buttons.** A tap zooms 1.25× instead of 2×, with a
  smooth animation, and quick taps add up. Holding + or − zooms continuously
  (it eases in, then about 2.6× per second) until you let go.
- The lit update button fades with the other buttons too.

### Fixed
- **Cyan flash at startup** (during "Starting audio"). Audio now starts while
  the screen is still dark. The display gets its memory priority before the
  first memory-heavy step, and the backlight waits a few more refreshes, so a
  clean frame is showing when it comes on.

## [v1.15.0] - 2026-10-29

### Added
- **Up to five saved Wi-Fi networks.** Joining a new network keeps the old
  ones. Settings → Wi-Fi lists them: tap one to connect, or the trash button
  to forget it. Networks nearby that are saved connect without the password.
  When the current network can't connect for 20 seconds, the radar tries the
  next saved one, and whichever connects is tried first from then on. They're
  in `settings.txt` too (`wifi2_ssid` ... `wifi5_pass`).
- **Wi-Fi messages:** *Connecting to ...*, *Connected to ...*, *Lost Wi-Fi -
  reconnecting*, and the reason when a connection fails.

### Fixed
- **Cyan flashes:**
  - **During updates and settings saves** (dozens during an update over
    Wi-Fi, one at startup). The screen's frame transfer is restarted by an
    interrupt after every frame, and that interrupt had to wait while the
    flash was being written. It now runs from internal RAM and keeps going
    during flash writes.
  - **Random flicker while the radar runs** (seen with the topo map). The
    display's memory reads competed equally with both CPUs, and heavy map
    work could starve them. They now have the highest priority on the
    memory bus.
  - The build now checks that the display interrupt code stays in internal
    RAM.

## [v1.14.0] - 2026-10-29

### Added
- **Update button on the radar** (the download arrow between Home and
  zoom-out):
  - The radar checks GitHub for a new version a minute after it starts and
    then every hour.
  - Normally the button is grayed out and fades with the others. Tap it to
    check right away.
  - When an update is available, it **lights up** and stays visible while
    the other buttons fade. Tap it, confirm, and the update installs (the
    same as from Settings → About).

### Changed
- **Only chip-v1 images are released now:** the boards in use all have
  ESP32-P4 revision v1.x, and the extra `chip-v3` files were confusing. File
  names keep `chip-v1`, so boards on v1.13.0 still find their updates.
- Release notes and the guide list exact file names and warn not to flash
  the `-ota.bin` update files over USB (that gives a black screen).

## [v1.13.0] - 2026-10-28

### Added
- **Updates over Wi-Fi:** Settings → About → **Check for updates** looks up
  the latest release on GitHub. **Install vX.Y.Z** downloads the right file
  for your board and chip and restarts into it, with a progress bar.
  Settings and favorites are kept.
  - The running version isn't touched until the new one has fully
    downloaded and passed its checks, so a power cut or dropped Wi-Fi just
    leaves the old version running.
  - A new version that crashes or resets within its first minute is rolled
    back automatically.
- Releases include `*-ota.bin` files: the update images the radar downloads.
  They're not for USB flashing.

### Changed
- The flash is split into two app slots (needed for updates over Wi-Fi).
  **Install v1.13.0 over USB once**; from then on, updates can come over
  Wi-Fi. Settings are kept: their flash area didn't move.

## [v1.12.1] - 2026-10-28

### Changed
- **More responsive menus and touch:**
  - Heavy background work (map tile downloads and decoding, the flight
    history of the selected aircraft) waits while you're touching the
    screen. It also stops while the menu is open, where neither is visible.
    It runs on the other core, but both cores share one memory cache, so it
    slowed the UI down too. Traffic updates still arrive, at most a few
    seconds late while you scroll.
  - The network stack's tasks stay on core 0, so they no longer take turns
    with the screen on core 1.

### Fixed
- The list of map tiles that failed to download grew without limit in the
  board's internal RAM (over days of use, or a long time offline). It's now
  capped. No other memory leaks were found: a long simulated session
  (menus, aircraft taps, zooming, theme and map changes) holds steady.
- v1.12.0 reported itself as 1.11.0 on the About page. The release build
  now stamps the version automatically.

### Added
- A `health:` line in the serial log every 30 s: free memory (and the lowest
  it has been), CPU load per core, the busiest tasks, and the longest the
  screen went without updating. It helps track down slowdowns.

## [v1.12.0] - 2026-10-28

### Fixed
- **Updates no longer erase your settings.** Earlier versions stored settings
  in a flash area that the release image (flashed at `0x0`) also covered, so
  every update wiped Wi-Fi, location, settings and favorites, despite what
  these notes said. Settings now live after the app, outside the image.
  Updating *to* v1.12.0 still starts fresh one last time (or restores from
  the SD card, see below); later updates keep everything.

### Added
- **`settings.txt` on the SD card:** every setting, Wi-Fi included, as
  commented `key = value` lines. Written whenever settings change, read back
  at startup, and editable on a computer. Settings survive even a full
  `erase_flash`, and moving the card to a second radar copies them.
- **`favorites.txt` on the SD card:** one aircraft per line (ICAO hex, label,
  type). Add aircraft on a computer by hex code or just by tail number; tail
  numbers are looked up online at startup and replaced by their hex code.
- Several tail-number lookups can now wait in line (before, a second one
  replaced the first).
- *Factory reset* also deletes `settings.txt` (`favorites.txt` is kept).

## [v1.11.0] - 2026-10-28

### Added
- **FAA sectional background map** (map button, or Display → Background
  map → FAA): VFR Sectional charts from the FAA's online chart service, in
  their own colors, dimmable. Aircraft labels get a dark backing on light
  maps. US only, not for navigation.
- JPEG map tiles are supported (the FAA serves charts as JPEG), using ChaN's
  TJpgDec.
- A `perf:` line in the serial log every 30 s: frame rate, frame/draw times
  and aircraft count, for diagnosing slowness.

### Changed
- **Smoother when zoomed out with lots of traffic:**
  - Aircraft symbols switch to a smaller set when zoomed out past about
    90 mi or with more than ~150 aircraft. They draw about twice as fast
    and are easier to tell apart.
  - Label placement only tries the 60 aircraft nearest the center. It used
    to check every aircraft against every other one, every frame.
  - With a background map: fewer recomposes while tiles stream in, fewer
    strips per tile at wide zooms, and cheaper scaling during pinches.
  - In the simulator benchmark (450 aircraft, 250 mi), the aircraft layer
    takes about 30% less time per frame, and pinch frames with a map
    25-40% less.

## [v1.10.0] - 2026-10-28

### Fixed
- **"API KEY REQUIRED" on the Dark streets map.** CARTO now requires a free
  API key for its maps. Enter one under Display → **CARTO key**, or put it
  in `carto_key.txt` on the SD card. Without a key, Dark streets now shows
  OpenStreetMap's standard map, darkened and tinted, instead of watermarked
  tiles. Watermarked tiles already saved on the SD card are no longer used;
  **Delete saved map tiles** frees their space.

### Added
- **Background map button** on the radar (lower right, next to Home):
  pick Off, Dark streets or Topo from a small menu.

### Changed
- The **Filters button** on the radar is now an eye, matching Settings.
- The **map credit** line fades out with the buttons and returns on any
  touch.
- **Screen orientation:** the default (0°) is now what used to be 180°, the
  way the board stands upright. Boards upgrading keep their current
  orientation; the menu value is converted (old 180° now shows as 0°).

## [v1.9.2] - 2026-10-28

### Changed
- The default home location is now **Roberts Field (RDM)**, Redmond,
  Oregon, instead of Amsterdam. Only new or reset boards use the default;
  a home you've set is kept.

## [v1.9.1] - 2026-10-28

### Added
- **Wake on alarm** (Clock & alarms), **off by default**: while the screen is
  asleep, alarms now pass silently and only a tap wakes it. Switch it on to
  have alarms ring and wake the screen as in v1.9.0.

## [v1.9.0] - 2026-10-28

### Added
- **Sleep button** (power symbol, left side of the radar): turns the screen
  off and mutes sounds. A single tap anywhere wakes it, without pressing
  anything underneath. Alarms still ring and wake it (from v1.9.1 only with
  Wake on alarm on).

## [v1.8.0] - 2026-10-28

### Added
- **Background maps** (Display → Background map): **Dark streets** (CARTO
  Dark Matter) or **Topo** (OpenTopoMap) under the radar.
  - Tinted to the theme color by default (topo inverted to stay dark), or
    the map's own colors; adjustable **Map brightness**; street-map place
    names can be switched off.
  - Tiles load closest-first with a blurry stand-in until each arrives;
    during pans and pinches the map moves with your fingers and is redrawn
    sharply afterwards.
  - The required map credit is shown at the bottom of the radar.
- **SD card support** with its own page (Settings → SD card): map tiles you
  view are saved on a FAT32 microSD card, so they load instantly and work
  offline for those areas. Shows card size and free space, saved tiles, and
  has delete, rescan and safe-unmount buttons. The card is optional.

### Notes
- The first view of a new area needs Wi-Fi; a screenful of tiles takes a few
  seconds to download.

## [v1.7.0] - 2026-10-28

### Added
- **Clock hand colors** (Clock & alarms): hue and saturation for the hour and
  minute hands and for the seconds hand.
- **Smooth seconds hand** option: sweeps instead of ticking.
- **Date window** switch (Clock & alarms).
- **Altitude colors in the Custom theme**: pick the hue at the ground, at
  10,000 ft and at 40,000 ft.
- **Altitude key**: a small color scale (0–40k ft) under the status pill
  when aircraft are colored by altitude. It fades out with the buttons.
- **Add a favorite by tail number** (Favorites → Add by tail number): type a
  registration or ICAO hex address; it's looked up on adsbdb.com.

### Fixed
- **Cyan flashes at power-up.** The backlight came on before the display was
  running, showing the panel's uninitialized picture. The bootloader now
  holds the backlight off, the screen memory is cleared, and the backlight
  turns on only after the boot screen has been drawn.

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

[v1.11.0]: https://github.com/ShadowStacker/P4-Radar/releases/tag/v1.11.0
[v1.10.0]: https://github.com/ShadowStacker/P4-Radar/releases/tag/v1.10.0
[v1.9.2]: https://github.com/ShadowStacker/P4-Radar/releases/tag/v1.9.2
[v1.9.1]: https://github.com/ShadowStacker/P4-Radar/releases/tag/v1.9.1
[v1.9.0]: https://github.com/ShadowStacker/P4-Radar/releases/tag/v1.9.0
[v1.8.0]: https://github.com/ShadowStacker/P4-Radar/releases/tag/v1.8.0
[v1.7.0]: https://github.com/ShadowStacker/P4-Radar/releases/tag/v1.7.0
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
