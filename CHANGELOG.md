# Changelog

All notable changes to Plane Radar P4. Versions match the
[GitHub releases](https://github.com/ShadowStacker/P4-Radar/releases).

**Which file do I flash?** Each release has four firmware images. Pick the one
for your board size **and** your ESP32-P4 chip revision. See
[Flashing](docs/USER_GUIDE.md#flashing-the-firmware) in the user guide.

From v1.12.0 on, upgrading keeps your Wi-Fi, home location, settings and
favorites. **Before v1.12.0 every update erased them** (see v1.12.0).

## [v1.39.1] - 2026-11-08

### Fixed
- **The 737-800 (and other busy types) looked like a generic aircraft.**
  The most common airliners still had the original ADS-B Radar icons, which
  beside the new ones looked generic. They now have icons in the same style
  as the rest of their family: 737-800/MAX 8 (also the P-8), A320/A320neo,
  787-9, CRJ900/1000, 767-300, A330-300, E195, E195-E2, ERJ 145,
  747-400 and classics, A380, A340-300, MD-11, C-17, C-130, C-27J,
  King Air 200, Beech 1900, Falcon 7X, Gulfstream V/G550, G150, Citation
  III, Eclipse, Dash 8-100, CL-415, Fokker 100, Tecnam P2006T, Sling.

### Changed
- **Checked against live traffic.** In a sample of about 4,500 aircraft
  from 32 busy areas worldwide, every type code now has an icon, and 98 %
  of them a type-specific one. The rest are ground vehicles, gliders,
  balloons and ultralights, which keep their class icons. Newly matched:
  ground stations (TWR, GND, SERV), Epic, Swift, Tecnam Sierra/Echo, VL3,
  M700, turbo Baron and Bonanza, Learjet 31/70, CJ1, motorgliders.
- `scripts/check_icon_coverage.py` repeats the check against live data.

## [v1.39.0] - 2026-11-07

### Added
- **Over 300 more aircraft icons, most drawn for one type.** There are now
  about 390 icons for about 690 type codes, all in the same style:
  - **Airliners, variant by variant:** A318/A319/A321, A220, the 737-200
    to -900, 757-200/-300, 767-200/-400, 777-200/-300/-300ER/777X,
    787-8/-10, A300/A310, A330-200/neo, A350-900/-1000, A340-600, 747-8 and
    747SP, Beluga, Dreamlifter, DC-10, TriStar, 717, MD-80/90, DC-9, 727,
    BAe 146, Tu-154, Il-62, Yak-42, CRJ200/700, E170 to E2, Superjet, ARJ21.
  - **Regional turboprops:** Dash 8-300 and Q400, ATR 42/72, Saab 340/2000,
    Do 328, Brasilia, Jetstream 41, F50, Dash 7, An-26, C-295, CN-235.
  - **About 40 business jets:** each Citation, Learjet, Phenom, Praetor,
    HondaJet (engines over the wing), Vision Jet, PC-24, Gulfstream, Global,
    Challenger and Falcon.
  - **Light aircraft:** Cessna 152 to 210, Cubs, Citabria, Husky, Maule,
    Piper singles and twins, Cirrus, Diamond, Bonanza (V-tail), Mooney, RVs,
    Lancair, TBM, PC-12, Kodiak, Beaver, Otter, Caravan, Air Tractor, Barons,
    Cessna twins, King Air 90/350, Metro, MU-2, Cheyenne, Piaggio Avanti,
    Skymaster, Islander, Do 228, CASA 212, Skyvan.
  - **Classics and warbirds:** DC-3, DC-4, DC-6, C-46, Constellation,
    Electra, Viscount, Ju 52, An-2, Beech 18, Staggerwing, Goose, Catalina,
    P-51, Spitfire, Hurricane, Bf 109, Fw 190, Zero, Corsair, P-47, P-40,
    P-38, B-17, B-24, B-25, B-29, Lancaster, Mosquito, T-6, T-28,
    Skyraider, Stearman, Tiger Moth, Pitts, Yak-52.
  - **Experimentals and aerobatics:** Long-EZ/VariEze/Cozy, Velocity,
    Extra, Sukhoi Su-26/29, CAP, light sport, CubCrafters, Glasair, ICON A5,
    Lake Buccaneer.
  - **Military:** F-86, MiG-15/21/29, F-104, Vampire, Hunter, L-39, A-4,
    F-4, F-14, F-15, F/A-18, F-22, F-35, Su-27 family, Tornado, Jaguar,
    Mirage, Draken, Viggen, Rafale, Gripen, Typhoon, J-10, F-16, F-5/T-38,
    Hawk, Alpha Jet, M-346, T-7, A-10, Harrier, F-117, SR-71, U-2, C-5,
    A400M, P-3, C-160, An-124, An-225, Il-76, Global Hawk, Reaper, KC-135,
    KC-46.
  - **50 helicopters** with their real blade count: Robinsons, Bell 206 to
    525, Huey, Cobra, MD 500/900, the Airbus range, Leonardo A109 to AW189
    and AW101, S-76, S-92, Black Hawk, CH-53, Sea King, Chinook, CH-46,
    Apache, Tiger, Mangusta, Mi-8/24/26/28, Kamov, NH90, Lynx, Enstrom,
    Schweizer, Cabri.

### Changed
- **Helicopter rotors look tidier.** Blades no longer line up with the nose
  or tail boom (where they looked like part of the body), and tandem-rotor
  helicopters show a V forward and a V back like a Chinook from above.
- **More internal RAM free.** The table of drawn icons is in PSRAM now
  (v1.37-1.38 kept it in internal RAM), and each icon is unpacked only when
  an aircraft of that type first appears.

## [v1.38.0] - 2026-11-07

### Changed
- **Icons that match what the aircraft really is.** 27 new icons, drawn in
  the same style as the ADS-B Radar set from each type's real span, length
  and layout. They cover:
  - **Propeller aircraft:** high- and low-wing singles (Cessna, Piper,
    Cirrus), single turboprops (PC-12, TBM, Caravan), piston twins (Baron,
    Seneca), twin turboprops (Beech 99, Metro, Jetstream) and high-wing
    twins (Twin Otter, Islander). Propellers are drawn as bars, so these no
    longer look like small jets.
  - **Helicopters by rotor layout:** two, three, four and five blades, large
    four-blade (UH-60, S-92), tandem (Chinook), attack (Apache) and coaxial
    (Kamov).
  - **Military:** single-fin, twin-fin and delta fighters, the A-10, jet
    trainers, the 707/KC-135 family, AWACS, E-2 Hawkeye, B-52, B-1, B-2 and
    MQ-9 Reaper.
- The type table now covers about 560 type codes (was 430). For example the
  BE99 is a twin turboprop and the PC-12 a single turboprop. A light aircraft
  with no type in the feed now shows as a high-wing single with a propeller.
- The user guide shows the whole icon set (docs/images/aircraft_icons.png).

## [v1.37.0] - 2026-11-07

### Changed
- **New aircraft icons that match the aircraft type.** About 40 detailed
  silhouettes: Cessna-style singles, King Air-style twins, Dash 8/ATR
  turboprops, Learjet, Gulfstream and Falcon business jets, CRJ, ERJ and
  E-Jets, A320 and 737 families, MD-80s, 757/767, 777, 787, A330/A350,
  A340, MD-11, 747, A380, C-130, heavy freighters, fighters, helicopters,
  tiltrotors, amphibians, gliders, ultralights, balloons, drones and ground
  vehicles. The radar picks the icon from the aircraft's type code (about 430
  known), and falls back to its size class when the type isn't known. Bigger
  aircraft get bigger icons. They still turn with the heading and take the
  altitude colors.
- Icons by ADS-B Radar for macOS - https://adsb-radar.com -
  https://apps.apple.com/app/id1538149835 (also credited on the About page).

## [v1.36.2] - 2026-11-07

### Fixed
- **Wi-Fi connecting only briefly, or not at all (v1.36.0 and v1.36.1).** The
  firmware is built with parts downloaded from Espressif's component library,
  and nothing pinned their versions, so each release took whatever was newest.
  v1.36.0 picked up a new Wi-Fi driver (esp-hosted 3.1.0) whose new receive
  buffers are too small for messages from this board's Wi-Fi chip. It dropped
  them ("SDIO RX read (2048 bytes) exceeds staging target") and lost the link
  within seconds of starting. Every part is now pinned to the exact versions
  of v1.35.2, so a release only changes what was changed on purpose.

## [v1.36.1] - 2026-11-07

### Changed
- **Trails fade with their aircraft in echo mode.** With the sweep on,
  **Echoes fade after the beam** on and trails on, each trail dims and
  brightens with its aircraft as the beam goes round, instead of staying at
  full brightness. Selected aircraft, emergencies and favorites keep bright
  trails, as their icons do. The fade comes in small steps, so a trail is
  only redrawn now and then, not on every frame. In this mode a traffic
  update also no longer rebuilds the whole background just for the trails.

## [v1.36.0] - 2026-11-07

### Added
- **Hidden aircraft.** A new button on the info card (crossed-out eye) hides
  an aircraft for good, after asking. It disappears from the radar at once
  and from then on is treated as if it wasn't in the data: not drawn, not in
  Nearby, no sounds or alerts of any kind (not even emergencies), not counted
  in the daily log or logbook, not recorded for replays. There is no limit on
  how many. **Settings → Hidden aircraft** lists them (100 to a page), with a
  button to bring each one back. The list is kept on the SD card as
  `hidden.txt`, which you can also edit on a computer.

### Changed
- The info card's buttons are slightly smaller to make room for the new one.
  A very long callsign is shortened with "...".

## [v1.35.2] - 2026-11-03

### Changed
- **Faster traffic decoding.** The feed is read directly, field by field,
  instead of first being turned into a full copy in memory. This is several
  times faster on a busy response (5–6× in tests with 400 aircraft), which
  helps most when zoomed far out over a busy area. It also uses less memory
  and leaves more time for the map and the screen. The results are the same:
  the tests compare it with the old decoder on thousands of generated and
  broken responses. The on-screen stats show the time as "decode … ms".

## [v1.35.1] - 2026-11-03

### Fixed
- **More room in internal RAM, so Wi-Fi doesn't run out.** Everything on
  the screen (buttons, labels, menu rows, their styles) used to be kept in
  the small internal RAM that Wi-Fi, secure connections and the drivers also
  need. A full menu page alone took a big share of it. All of that now goes
  in the large external memory (PSRAM) instead. Internal RAM is only used if
  PSRAM is ever full. With the on-screen stats on, the "RAM" figure should be
  noticeably higher, especially with a menu open, and its "low" mark should
  stay well above 0.

## [v1.35.0] - 2026-11-03

### Added
- **Your own sounds.** WAV files in a `sounds` folder on the SD card replace
  the built-in sounds: `aircraft_detected.wav`, `first_time.wav`,
  `proximity.wav`, `emergency.wav`, `favorite_seen.wav`, `favorite_near.wav`,
  `watchlist.wav`, `alarm.wav`, `startup.wav`, `home.wav`, `click.wav` and
  `ping.wav`. Any ordinary WAV works (8 to 32-bit, float, mono or stereo, any
  sample rate). The bottom of the Sound page lists what it found and whether
  each file worked, with **Load them again** after you change the card. Delete
  a file to get the built-in sound back. The files are read in the background
  and kept in the large external memory, so they don't take the memory Wi-Fi
  needs.

## [v1.34.1] - 2026-11-03

### Changed
- **Logbook has its own entry in Settings**, after Daily log, instead of
  being inside the Daily log page.
- **The "First time here" sound switch is on the Sound page** now (with a
  test button), next to the other sounds. The Logbook page keeps the switch
  for the announcement itself.
- The **New aircraft** sound is now called **Aircraft detected** (Sound page,
  phone page and guide). The setting is the same, so nothing needs redoing.

## [v1.34.0] - 2026-11-03

### Added
- **Place search on the Location page.** A search bar at the top: type a
  city, state, country or any place name and pick from up to six matches,
  each with where it is and what it is. Tap one to **show it on the radar**
  (zoomed to fit it), **set it as home**, or **bookmark** it (the name is
  filled in). It uses OpenStreetMap's Nominatim, so it needs Wi-Fi.

## [v1.33.0] - 2026-11-03

### Added
- **Bookmarks**, replacing Saved places: their own page (Settings →
  Bookmarks), and a **bookmark button on the radar** (left side, between the
  power and airplane buttons). Tap one to show it on the radar at its zoom;
  the trash can removes it. Up to **eight** now (was four). Your saved places
  carry over as bookmarks.
- **Long-press anywhere on the radar** now asks what to do with that spot:
  **Set as home**, or **Bookmark** it (you name it; leave it blank for
  "Bookmark N").

### Changed
- The left-side radar buttons are spaced a little closer to make room for
  the bookmark button.
- On the phone page, the place slots are called Bookmark 1 to 8.

## [v1.32.6] - 2026-11-03

### Fixed
- **The real cause of the Wi-Fi dropouts: the board ran out of internal
  RAM.** Your serial log showed it at 0 KB free, then the Wi-Fi link
  couldn't get memory for its transfers ("mempool OOM"), secure connections
  failed, and the Wi-Fi chip stopped answering. The screen's drawing buffer
  took half of the internal RAM left after the firmware itself; it is now
  half the size, which frees 80 KB (72 KB on the 4" board) and more than
  doubles the room for Wi-Fi, menus and downloads. Full-screen redraws are
  drawn in more, smaller strips, which should not be noticeable.

## [v1.32.5] - 2026-11-03

### Fixed
- **The automatic recovery now also covers Wi-Fi that can't reconnect.** If
  Wi-Fi had been working and then can't get back on (for example, it keeps
  trying your home network and the other saved networks in turn), the radar
  restarts after 5 minutes, which also resets the Wi-Fi chip. Before, it
  only did this while Wi-Fi still showed as connected, so it could keep
  cycling through the saved networks forever. It still restarts at most
  twice in a row, and never when Wi-Fi hasn't connected since the radar
  started (a wrong password, or out of range).

## [v1.32.4] - 2026-11-03

### Fixed
- **Wi-Fi dying, especially with the on-screen stats turned on.** Measuring
  CPU use per task paused the processor for a moment while it checked every
  task's memory. With the stats on, that happened every second; with them
  off, every 30 seconds for the health log. Those pauses broke the link to
  the Wi-Fi chip, so the data stopped and the screen stuttered. The stats
  now read only quick counters. The serial health log no longer shows the
  "largest block" figure, which needed the same kind of pause.

## [v1.32.3] - 2026-11-03

### Added
- **Automatic recovery when the internet stops answering.** Sometimes Wi-Fi
  says it is connected but nothing gets through. If no request has had an
  answer for a minute, the radar rejoins the Wi-Fi network (again every
  2 minutes while it lasts). After 5 minutes it restarts, which also resets
  the Wi-Fi chip. A notice says what it is doing. It restarts at most twice
  in a row: if your router itself has no internet, the radar just keeps
  rejoining instead of restarting over and over.

### Changed
- **The status pill says why the data is old**, for example "Data 85s old -
  No connection", "- Rate limited" or "- HTTP 503", instead of only the age.

## [v1.32.2] - 2026-11-02

### Fixed
- **The screen, menus and live data freezing for many seconds at a time.**
  The screen asked the Wi-Fi chip for its signal strength every 5 seconds
  and waited for the answer. When the link to the Wi-Fi chip was busy, that
  could take up to 5 seconds. Everything else that needed the Wi-Fi status
  (the screen, the feed, the phone page) waited too, so it could seize up
  almost completely. Reconnecting after a drop also waited on the clock
  that keeps the screen ticking. A separate background task now does all
  the talking to the Wi-Fi chip; the screen and the feed only read the last
  known status and never wait.

## [v1.32.1] - 2026-11-02

### Fixed
- **Card writes no longer hold up the live feed and the map.** Saving map
  tiles, logbook lines, replay frames, the daily log, saved lookups and
  screenshots now happens on its own low-priority task. A slow SD card no
  longer pauses aircraft updates or tile downloads. If the card falls behind,
  new map tiles are shown but not saved, and are downloaded again next time.

## [v1.32.0] - 2026-11-02

### Added
- **An airplane button on the radar** (left side, opposite the camera) opens
  the Nearby aircraft list.
- **Daily log by the hour.** Today's aircraft per hour as bars and the
  busiest hour, plus totals for the last 7 and 30 days (different aircraft,
  the busiest day and hour). The phone page shows them too and downloads the
  last 90 days as a CSV file.

## [v1.31.0] - 2026-11-02

### Added
- **Nearby aircraft.** A list in Settings of the aircraft around, with type,
  distance from home, altitude and speed, sorted by distance, altitude or
  speed. Tap one to see it on the radar, centered, with its info card.

## [v1.30.0] - 2026-11-02

### Added
- **Saved places.** Up to four places besides home on the Location page:
  pan and zoom the radar to a spot, save it with a name, and tap it later to
  fly there. They're also in `settings.txt` (`place1` to `place4`) and on the
  phone page.

### Changed
- **The sweep can go as slow as 1 rpm** (it stopped at 4), and new radars
  start at 4 rpm instead of 15. A radar that's already set up keeps its
  speed.

## [v1.29.0] - 2026-11-02

### Added
- **Logbook.** Every aircraft seen near home is written down in
  `logbook.csv` on the SD card, one line per visit. A new Logbook page (from
  Daily log) and the phone page's Daily log tab show the totals, the most
  frequent visitors and the newest ones, and the phone page downloads the
  file.
- **"First time here" alert.** An aircraft the logbook has never seen is
  announced, with its own sound (after the logbook's first two days).
  Either can be turned off on the Logbook page.

## [v1.28.0] - 2026-11-02

### Added
- **Flight replay.** With an SD card, the traffic around home is recorded
  every 15 seconds (a week is kept). **Settings → Replay a day** plays a day
  back on the radar, with a time slider, play/pause and speeds up to 300×.
  Live traffic and alerts pause while a replay plays. Recording can be turned
  off on the same page.

## [v1.27.0] - 2026-11-02

### Added
- **Backup and restore.** A new **Backup** tab on the phone page downloads
  one file with all settings, saved Wi-Fi networks, favorites and silenced
  aircraft, and restores it on the same radar or a new one.

## [v1.26.3] - 2026-11-02

### Changed
- **Map tiles show up as they arrive.** The background is rebuilt in eight
  small steps, and a new tile used to start that over; while tiles kept
  coming (every quarter second) nothing new reached the screen until the
  last one was in. Now a rebuild that's under way finishes and shows first,
  and the next one brings what arrived meanwhile.
- **The version is shown at the bottom of the loading screen.**

## [v1.26.2] - 2026-11-02

### Fixed
- **Thin black lines between map tiles.** Tiles were scaled and placed with
  rounding that could leave them a pixel short of each other, and the soft
  edge of a scaled tile let the black background through. Tiles now meet on
  whole pixels, and each one's edge pixels are covered by its neighbor. The
  simulator checks every tile edge at several zooms for dark lines.

## [v1.26.1] - 2026-11-02

v1.23.0 to v1.26.0 didn't stay installed on the board (they went back to
v1.22.1). To find out why, this version is v1.22.1 with only one change from
them; the system log, backup and flight replay are left out for now.

### Changed
- **Less redrawing when little moves.** Aircraft are drawn at whole pixels,
  and only those that moved by a pixel (or whose label changed) are redrawn.
  The on-screen stats show how often a frame had nothing to redraw (*rest*).

### Fixed
- **A possible crash with big range rings.** LVGL's circle drawing writes past
  its memory for some radii above about 920 pixels (rings when the view is far
  from home). Such rings are now drawn as short lines.

## [v1.22.1] - 2026-11-01

### Changed
- **Smoother radar when traffic arrives.** The background layer (map, rings,
  trails) is rebuilt in eight small steps instead of four, so no single frame
  carries much of it.
- **Settings from the phone apply without a freeze.** Only a new theme, colors
  or sizes rebuild the screens; anything else just updates the radar, and an
  open settings page shows the new value where it is. Aircraft no longer
  vanish for a few seconds after such a change (only new filters start over).
- **Less redrawing of unchanged text.** The status pill, info card and
  settings pages leave text alone when it hasn't changed.

## [v1.22.0] - 2026-11-01

### Changed
- **Smoother settings menus.** Rows are flat, with a thin line between them,
  instead of rounded panels: about a quarter less drawing work while
  scrolling (a third on the main page). Unselected choice buttons are
  outlines.
- **The Display page is split up.** It keeps the basics (theme, brightness,
  units, rotation) and links to Aircraft & labels, Map & airports, Radar
  sweep, and Dimming & night, so each page opens and scrolls faster.

## [v1.21.3] - 2026-11-01

### Fixed
- **Fewer freezes in the menus.** Settings were saved to flash once a minute
  while the menus were open, which stops the whole chip for a moment; now
  they're saved when you leave the menus.

### Changed
- **Faster screen handoff when mounted upside down** (the usual 0 deg
  setting). Turning the picture is now done in place, in the chip's fast
  internal memory, instead of through a buffer in the slower external memory
  the screen also reads from.

## [v1.21.2] - 2026-11-01

### Fixed
- **Cyan flashes are back to gone.** Drawing on both processor cores (from
  v1.20.0) overloaded the memory the screen reads its picture from, and the
  screen showed cyan when it fell behind (every few seconds, when new traffic
  arrived). Drawing is on one core again; the faster partial redraws stay.

## [v1.21.1] - 2026-11-01

### Changed
- **Updates get the radar to themselves.** While a new version downloads
  and installs (from GitHub or a phone), the radar stops drawing, fetching
  traffic, loading map tiles and writing to the SD card; it all carries on
  if the update fails.

## [v1.21.0] - 2026-11-01

### Added
- **What's new after an update.** The radar shows a short list of what
  changed, once, after it starts a new version (with the notes of every
  version since the one it ran before). **About → What's new** shows it again.

### Fixed
- **On-screen stats could make the screen stutter or freeze.** The CPU and
  memory figures are now gathered by a small background task instead of the
  task that draws the screen.

## [v1.20.0] - 2026-10-31

### Changed
- **Faster drawing.** The radar now redraws only what changed each frame
  (the sweep's wedge, and each aircraft and label where it was and where it
  is) instead of the whole screen: typically a quarter to a half of the
  screen, depending on traffic and the sweep. Every frame is checked in the
  simulator to match a full redraw pixel for pixel.
- **Both processor cores draw.** LVGL hands each frame's drawing to two draw
  threads, so the core that mostly waits for the network helps with drawing.

### Added
- **More on-screen stats** (About → On-screen stats): how much of the screen
  each frame redraws, drawing vs. handing the picture to the screen (and the
  rotation), the radar's own drawing and background rebuilds, and the time to
  decode a traffic update. See the user guide for what each line means.

## [v1.19.0] - 2026-10-31

### Added
- **Silenced aircraft**, the opposite of favorites: a mute button on the
  aircraft info card. A silenced aircraft no longer plays the new-aircraft
  chime, the proximity alert or the watchlist alert; emergency squawks still
  sound. Up to 32, listed under Sound → Silenced aircraft (and on the phone
  page's Favorites tab) with a button to unmute each. Silencing a favorite
  removes it from the favorites, and starring a silenced aircraft unmutes it.

## [v1.18.3] - 2026-10-29

### Added
- **The radar's own hotspot** for the phone page (Settings → Phone settings
  page → Start the radar's hotspot), for when the phone can't reach the radar
  through the router. One QR code joins the phone to the radar's Wi-Fi, a
  second one opens the page. The radar stays on your Wi-Fi meanwhile, and the
  hotspot turns off by itself after 30 minutes without a phone.

## [v1.18.2] - 2026-10-29

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

## [v1.18.1] - 2026-10-29

### Fixed
- Wi-Fi power saving is off: asleep between beacons, the radio could be slow
  to notice (or miss) a phone opening the settings page.

### Added
- The radar's Phone settings page says whether the page is working: **Server**
  (the radar asks its own page) and **Phone visits** (requests that reached the
  radar, and from where), to tell a network problem from a radar problem.
  Every request is also in the serial log (`web: GET / from ...`).

## [v1.18.0] - 2026-10-29

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
