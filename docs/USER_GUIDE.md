# Plane Radar P4 — User Guide

This guide covers every screen and setting on the device, plus flashing and
troubleshooting. For what changed in each version, see the
[changelog](../CHANGELOG.md).

**Contents**

- [First start](#first-start)
- [The radar screen](#the-radar-screen)
  - [What you're looking at](#what-youre-looking-at)
  - [Touch gestures](#touch-gestures)
  - [Buttons](#buttons)
  - [The aircraft info card](#the-aircraft-info-card)
  - [Flight history](#flight-history)
  - [Clock and alarms](#clock-and-alarms)
- [Settings reference](#settings-reference)
  - [Wi-Fi](#wi-fi) · [Location](#location) · [Display](#display) ·
    [Sound](#sound) · [Traffic & data](#traffic--data) · [Filters](#filters) ·
    [Favorites](#favorites) · [About](#about)
- [Alerts explained](#alerts-explained)
- [Flashing the firmware](#flashing-the-firmware)
- [Troubleshooting](#troubleshooting)

---

## First start

1. **Wi-Fi.** On first boot the device opens the Wi-Fi page and scans for
   networks. Tap yours and type the password on the on-screen keyboard (the
   eye button shows what you've typed). Open networks connect right away.
   For a hidden network, use **Hidden network...**.
2. **Location.** Next it shows the Location page. Pick one of:
   - **Pick on the map**: drag the map until the crosshair is on your house,
     then tap **Set home**.
   - **Enter coordinates**: latitude, then longitude, in decimal degrees.
     West and south are negative (use the ± key).
   - **Use an airport**: type a code like `KRDM`, `RDM`, `BDN` or `S39`.
3. Tap **X** to go to the radar. Traffic appears within a few seconds.

You can redo any of this later from **Settings** (the gear button).

---

## The radar screen

### What you're looking at

| Element | Meaning |
|---|---|
| **Home marker** (white ring with a dot) | Your home location. Range rings and the sweep are centered on it. |
| **Range rings** | Distance from home, labeled in your units (km, mi or nm). Spacing adapts to the zoom level. |
| **Compass bezel** | North is always up. N/E/S/W and every 30° are marked around the edge. |
| **Aircraft symbols** | Point the way the nose is facing. Shapes: light aircraft, jets, heavies, helicopters, ground vehicles. |
| **Colors** | With *Aircraft colors → By altitude*: orange (low) → green → blue → magenta (high). Otherwise the theme color. |
| **Labels** | Line 1: callsign (or registration). Line 2: altitude, a climb/descend arrow (over 400 ft/min) and the type code. Altitude is in feet below 18,000 ft, e.g. `4500`, and as a flight level above, e.g. `FL350`. `GND` = on the ground. |
| **Speed vector** (thin line ahead of an aircraft) | Where it will be in **one minute** at its current speed and track. |
| **Trail** | Where the aircraft has been over the last few minutes, colored by altitude at each point. |
| **Rim dots** | Aircraft beyond the edge of the screen, shown at the correct bearing. |
| **Red pulsing ring** | Emergency: squawk 7500 (hijack), 7600 (radio failure) or 7700 (general emergency). |
| **Gold ring + star** | One of your [favorites](#favorites). |
| **Runways and codes** | Airports from the built-in database. Large airports show below 250 km of zoom, medium below 70 km, small fields below 35 km. Faint thin runways are unpaved, or placed approximately (strips with no surveyed coordinates). Codes are ICAO (`KRDM`), but fields with only a local code use their IATA code if they have one (Prineville `S39` shows as `PRZ`). See **Airport code style**. |
| **Status pill** (top) | Aircraft count, filtered count, zoom radius, and any problem (Wi-Fi, feed errors, stale data). Tap it: it opens Wi-Fi when offline, About when online. |

Between data updates (every 5 s by default) aircraft keep moving along their
track at their reported speed, so motion stays smooth. An aircraft the feed
stops reporting disappears after 45 seconds.

### Touch gestures

| Gesture | Action |
|---|---|
| **Drag** with one finger | Pan the map. Flick it and it keeps gliding. |
| **Pinch** with two fingers | Zoom in or out around the point between your fingers. |
| **Double-tap** | Zoom in 2× on that spot. |
| **Tap an aircraft** | Open its [info card](#the-aircraft-info-card). Tap empty map to close it. |
| **Long-press** the map | Asks whether to make that spot your home location. |

Zoom ranges from a 1 km to a 460 km radius. The radar fetches traffic for
whatever area you're looking at, up to the feeds' 250 nm limit.

### Buttons

| Button | Position | Action |
|---|---|---|
| ⚙ Gear | top right | Open Settings |
| ⏷ Funnel | right | Open the [Filters](#filters) page. Lit while a filter is hiding aircraft. |
| 🔊 Speaker | left | Mute or unmute all sounds (same as **Sound → Sound**) |
| 🕘 Clock | right, below the funnel | Switch the bezel between compass headings and a [clock face](#clock-and-alarms) (lit = clock) |
| ⟳ Loop | top left | Turn the radar sweep animation on or off (lit = on) |
| ＋ / − | lower right / lower left | Zoom in / out by 2× |
| ⌂ Home | bottom | Fly back to home at your **Home zoom**. It's dimmed when you're already there. |

With **Auto-hide buttons** on (the default), the buttons and the status pill
fade out after 10 seconds without a touch. Touch anywhere to bring them back.
They stay visible while an info card is open, and while the status pill is
reporting a problem (offline, feed errors).

### The aircraft info card

| Field | Meaning |
|---|---|
| Callsign + type badge | Flight number (or registration) and ICAO type code, e.g. `B738`. **MIL** = military. |
| Route | Where the flight is from and going, e.g. *Dublin (DUB) > Lanzarote (ACE)*. Shown when the route is known. |
| Airline line | The airline (or the registered owner for private and military aircraft) and the exact model, e.g. *Ryanair - Boeing 737-8AS* |
| Next line | Registration, ICAO hex address, emitter category (e.g. `cat A3`), and the [flight history](#flight-history) status |
| ALT | Barometric altitude in feet (or "Ground") |
| V/S | Vertical speed in ft/min (+ climbing, − descending) |
| SPEED | Ground speed in knots |
| TRACK | Direction of travel in degrees |
| FROM HOME | Distance and bearing from your home |
| SQUAWK | Transponder code |

Card buttons:
- ★ **Star**: save or unsave as a [favorite](#favorites). Gold = saved.
- ➤ **Follow**: keep this aircraft on screen. The map re-centers whenever it
  drifts away from the middle. Panning or pinching stops following.
- ✕ **Close**: close the card.

**Where the airline and route come from.** When you tap an aircraft, the radar
looks it up on [adsbdb.com](https://www.adsbdb.com), a free database of
airframes and callsign routes. Routes are matched by flight number from a
community-maintained list, so occasionally they are out of date (a flight
number reused for a different route) or missing, mostly for private,
military and charter flights. Results are remembered for the last 24 aircraft.
The card says *Looking up airline and route...* while it waits, and
*No airline or route info* if adsbdb knows nothing. Turn it off with
**Display → Airline and route on tap**.

### Flight history

When you tap an aircraft, the radar downloads its **flight history** and
draws it as a thicker line behind the aircraft, colored by altitude like the
trails. A small ring marks where the flight started (its takeoff, if the
feed saw it). Zoom out to see the whole route.

- The card's second line shows the progress: *loading history...*, then
  *history 1h 42m* (how much of the flight is shown), or *no history*.
- Only the current flight is shown. If the aircraft landed and took off again
  today, the earlier legs are left out. At most the last 14 hours are drawn.
- The history refreshes every 2 minutes while the card is open, so it stays
  joined to the live trail.
- The history comes from the globe map of the selected **ADS-B feed** (with
  the other two feeds as backups). These history files aren't an official
  API, so a feed may stop serving them; the card then says *no history* and
  the normal trail still works.
- Turn it off with **Display → Flight history on tap**.

### Clock and alarms

Tap the clock button to turn the radar into a clock. The compass headings
around the edge are replaced by the numbers 1–12 with minute marks, and hour,
minute and (optionally) seconds hands sweep from the middle, under the
aircraft. A date window sits at three o'clock. The radar keeps working as
usual. Tap the button again for the compass.

The time comes from the internet (NTP), so after power-up the clock needs
Wi-Fi; until then the face says *Waiting for the time...*. Set your time zone
in **Settings → Clock & alarms**. Daylight saving time is handled
automatically.

When an alarm goes off, a full-screen notice shows the time with **Snooze 9
min** and **Stop**. The screen wakes up if it was dimmed, and the alarm beeps
every 2 seconds at no less than 70 % volume, **even if sound is switched
off**. It stops by itself after 10 minutes.

---

## Settings reference

Open with the gear button (or the funnel button for Filters).

- Changes show up right away, so you can see what they do.
- **<** goes back a level. On the main settings page it returns to the radar
  and **keeps** your changes.
- **X** returns to the radar and **discards** everything you changed since you
  opened settings: options, home location, filters, theme and removed
  favorites. It says *Changes discarded*. Two things can't be undone this way:
  Wi-Fi (joining or forgetting a network happens right away) and a restart you
  confirmed (screen rotation, erase settings).

### Wi-Fi

| Option | What it does |
|---|---|
| Status | Current network, IP address and signal strength (dBm, closer to 0 is better) |
| Network list | Tap a network to join it. Bars are colored by signal; the eye icon marks secured networks. |
| Scan again | Refresh the list |
| Hidden network... | Type a network name (SSID) that isn't broadcast, then its password |
| Forget saved network | Disconnect and erase the saved credentials |

If the connection drops, the device retries by itself, waiting longer between
attempts (up to every 30 s).

### Location

| Option | What it does | Default |
|---|---|---|
| Latitude / Longitude | Your current home point | Amsterdam |
| Pick on the map | Closes settings and shows a crosshair. Drag the map under it, then tap **Set home**. This also saves the current zoom as the Home zoom. | |
| Enter coordinates | Type latitude (−90 to 90), then longitude (−180 to 180) | |
| Use an airport | Jump home to an airport by ICAO (`KRDM`), IATA (`RDM`) or FAA code (`S39`, `BDN`) | |
| Home zoom (radius) | How far the view reaches when you press Home, from the center to the edge of the screen | 25 km |

Home is the center of the range rings and the sweep, and the reference for
proximity and favorite alerts.

### Display

| Option | What it does | Default |
|---|---|---|
| Theme | **Green** (classic phosphor), **Amber**, **Blue**, **Night** (dim red to preserve night vision; aircraft colors stay red), **Custom** (your own colors, see below) | Green |
| Custom colors | Only with the Custom theme: opens the [Custom colors](#custom-colors) page | |
| Brightness | Backlight level, 5–100 % | 80 % |
| Distance units | km, statute miles (mi) or nautical miles (nm), for rings, the zoom readout and distances | mi |
| Screen rotation (clockwise) | Turn the whole picture by 0°, 90°, 180° or 270°, e.g. when the board is mounted with its USB port on another side. North stays at the top of the rotated screen and touch follows the rotation. Asks to restart, which applies it. If you skip the restart, it applies at the next power-up. | 0° |
| Auto-hide buttons after 10 s | Fade the radar buttons out when idle; touch to show them again | On |
| Labels | **Off**: no labels, except the selected aircraft, emergencies and favorites. **Callsign**: one line. **Full**: callsign + altitude/climb/type. | Full |
| Aircraft colors | **Theme**: one color. **By altitude**: color gradient by altitude. | By altitude |
| Aircraft size | Size of the aircraft symbols, 60–160 % | 100 % |
| Label size | **Small**, **Normal** or **Large** text for aircraft labels | Normal |
| Trails | Show where aircraft have been (the last few minutes) | On |
| Flight history on tap | Download and draw the whole current flight of the aircraft you tap. See [Flight history](#flight-history). | On |
| Airline and route on tap | Look up the airline, origin, destination and model of the aircraft you tap, see [the info card](#the-aircraft-info-card) | On |
| Speed vectors (1 min) | Line showing each aircraft's position one minute ahead | On |
| Range rings | Distance rings and crosshair around home | On |
| Runways | Draw airport runways | On |
| Airport codes | Label airports with their code | On |
| Airport code style | **ICAO** (`KRDM`; fields with only a local code such as `S39` use their IATA code, `PRZ`) or **IATA** (`RDM`, wherever one exists) | ICAO |
| Radar sweep | The rotating beam animation | On |
| Sweep speed | Revolutions per minute, 4–60 | 15 rpm |
| Sweep trail | Length of the glowing trail behind the beam, 0–180°. **0 = clean bar** (just the beam line). | 70° |
| Smooth trail fade | Draws the trail in finer steps with a softer, longer fade (no visible bands). Uses slightly more processing. | Off |
| Echoes fade after the beam | Aircraft dim after the beam passes and brighten on the next pass, like a real radar scope. Selected aircraft, emergencies and favorites stay bright. | Off |
| Dim when idle | Lower the backlight after Never / 1 / 5 / 15 / 30 minutes without a touch. Touch to wake. | Never |
| Dimmed brightness | Backlight level while dimmed, 0–50 % | 10 % |

#### Custom colors

Pick **Custom** as the theme, then tap **Custom colors**. Four swatches at the
top preview the radar color, background, aircraft and labels as you move the
sliders. The whole screen switches to the new colors when you let go of a
slider.

| Option | What it does | Default |
|---|---|---|
| Radar color: hue / saturation | The main color: sweep, rings, compass, trails and buttons. Hue goes around the color wheel (0 red, 120 green, 200 blue). Saturation 0 is gray. | 175°, 80 % (teal) |
| Background brightness | 0 = black; higher values give a tinted background | 6 % |
| Rings and compass brightness | How bright the range rings and compass marks are | 45 % |
| Aircraft color: hue / saturation | Aircraft symbol color when **Aircraft colors** is set to **Theme**. Saturation 0 = white. | White |
| Label color: hue / saturation | Aircraft label text color. Saturation 0 = white. | White |
| Reset custom colors | Back to the defaults above | |

### Clock & alarms

| Option | What it does | Default |
|---|---|---|
| Time now | The current local time and date, or *Not set yet* before the clock has synced over Wi-Fi | |
| Time zone | Your zone, chosen from a list of 24 (US zones, Europe, Asia, Australia, ...). Daylight saving is automatic. | Pacific |
| Clock face on the radar | Same as the clock button | Off |
| 24-hour time | 19:45 instead of 7:45 PM, on the alarm screens and settings | Off |
| Seconds hand | Show the red seconds hand on the clock face | On |
| Alarms 1–4 | Each shows its time and days. The switch turns it on or off; tap the row to edit it. | All off, 7:00 AM Mon–Fri |
| Cancel snooze | Shown while an alarm is snoozed, with the minutes left | |

**Editing an alarm:** **Hour** and **Minute** sliders (changing them switches
the alarm on), **Alarm on**, the days it repeats (tap **Mo**…**Su**), and
**Test this alarm**. With no days selected the alarm rings once and then
switches itself off.

### Sound

| Option | What it does | Default |
|---|---|---|
| Sound | Master switch for all sounds | On |
| Volume | Speaker volume, 0–100 %. A chime plays when you let go of the slider. | 60 % |
| Touches | Click on button presses | On |
| New aircraft | Soft chime when 1–3 new aircraft appear. It stays quiet for big batches, e.g. after you pan to a new area. | On |
| Each sweep (sonar ping) | Sonar ping once per sweep revolution | Off |
| Emergency squawks | Siren when an aircraft squawks 7500/7600/7700 or declares an emergency | On |
| Proximity alert | Three beeps when **any** airborne aircraft comes within the alert radius of home, below the altitude limit | **Off** |
| Alert radius | Proximity distance from home, 1–20 (your units) | 3 km |
| Below altitude | Only alert for aircraft below this altitude, 500–20,000 ft | 5,000 ft |
| Favorite detected | Fanfare when a saved aircraft comes into range | On |
| Favorite nearby | Two-tone chime when a saved aircraft comes within the nearby radius | On |
| Nearby radius | Distance from home for "Favorite nearby", 1–100 (your units) | 15 km |
| Test | Play each sound | |

See [Alerts explained](#alerts-explained) for exactly when each one fires.

### Traffic & data

| Option | What it does | Default |
|---|---|---|
| ADS-B feed | Where aircraft data comes from: **adsb.fi**, **airplanes.live** or **adsb.lol**. All three are free community networks with similar coverage. Try another if one is slow or down. | adsb.fi |
| Update every | How often to fetch new positions, 2–30 s. Aircraft move smoothly in between either way. Please stay at 5 s or more for everyday use; these are free services. | 5 s |
| Filters... | Shortcut to the Filters page | |

### Filters

Filters decide which aircraft appear on the radar. **Favorites are always
shown**, whatever the filters, and their alerts still work. When filters are
hiding aircraft, the status pill shows how many, e.g. "3 aircraft (11
filtered)". Changes take effect at once.

| Option | What it shows / hides | Default |
|---|---|---|
| **Aircraft kinds** | One switch per kind, based on the category the aircraft broadcasts: | all on |
| · Light aircraft | Small piston aircraft (Cessna, Piper…) and ultralights | |
| · Small & business | Business jets and turboprops | |
| · Airliners | A320/737-size airliners | |
| · Heavies (757, widebody) | 757 and widebodies (777, 787, A350, 747…) | |
| · Fast jets | High-performance aircraft (mostly military fighters) | |
| · Helicopters | Rotorcraft | |
| · Gliders, balloons, drones | Gliders, balloons, UAVs, parachutists | |
| · Unknown category | Aircraft that don't broadcast a category. Quite a few don't; turn this off only to hide them. | |
| Military aircraft only | Hide everything not flagged as military in the feed's database | Off |
| Aircraft on the ground | Include taxiing and parked aircraft (shown in grey) | Off |
| Only types | Comma-separated ICAO type codes. A `*` at the end matches a prefix: `A32*` = the A320 family, `B7*` = Boeing 7-series. Empty = all types. | all |
| Hide below / Hide above | Altitude band, in 500 ft steps (up to 40,000 / 60,000 ft) | no limit |
| Slower than / Faster than | Ground-speed band in knots, e.g. hide anything slower than 100 kt to drop light traffic | no limit |
| Reset all filters | Back to showing everything (airborne) | |

Filters combine. For example, *Helicopters* only + *Hide above 3,000 ft* shows
low-flying helicopters.

### Favorites

Save an aircraft by tapping it and pressing ★ on its info card, up to 32.
Favorites are tracked by the airframe's **ICAO address**, which never changes,
so a saved aircraft is still recognized under a different flight number.

On the radar, favorites get a gold ring, a star and an always-on gold label.
Off-screen, they show as a gold rim dot.

The Favorites page lists each saved aircraft with its type and ICAO address.
It also says whether it's **in range** right now and how far away it is, and
has a remove button.

Favorites are only "detected" inside the area the radar is fetching: roughly
what's on screen plus a margin, up to 250 nm. Zoom out to watch a wider area.

### About

| Item | Meaning |
|---|---|
| Firmware | Installed version |
| Display | Panel resolution (800×800 = 3.4", 720×720 = 4") |
| Wi-Fi | IP address and signal strength |
| Feed | Aircraft count and seconds since the last successful update, or the last error |
| Free memory | Internal RAM and PSRAM free (useful for bug reports) |
| Restart | Reboot the device |
| Factory reset | Erase Wi-Fi, location, settings and favorites, then reboot |

---

## Alerts explained

Each data update plays at most **one** sound, the most important one, in this
order:

1. **Emergency squawks**: an aircraft starts squawking 7500/7600/7700 or
   reports an emergency.
2. **Favorite nearby**: a favorite enters the Nearby radius around home. It
   plays once per approach; the aircraft has to leave the radius before it can
   trigger again.
3. **Favorite detected**: a favorite appears in the data. This also happens
   right after power-on, and again if it disappears for over 45 s and comes
   back.
4. **Proximity alert** (off by default): any airborne aircraft below the
   altitude limit enters the alert radius. Once per approach.
5. **New aircraft**: 1–3 aircraft appeared that weren't there before.

Favorite events also show an on-screen notice ("RCH471 is nearby") even when
their sound is off. The **Sound** master switch silences everything.

---

## Flashing the firmware

Each [release](https://github.com/ShadowStacker/P4-Radar/releases) has four
images. Pick by board **and** chip revision:

| File | Board | Chip |
|---|---|---|
| `plane-radar-3.4in-800x800-chip-v3-*.bin` | ESP32-P4-WIFI6-Touch-LCD-3.4C | revision v3.x |
| `plane-radar-3.4in-800x800-chip-v1-*.bin` | ESP32-P4-WIFI6-Touch-LCD-3.4C | revision v0.x / v1.x |
| `plane-radar-4in-720x720-chip-v3-*.bin` | ESP32-P4-WIFI6-Touch-LCD-4C | revision v3.x |
| `plane-radar-4in-720x720-chip-v1-*.bin` | ESP32-P4-WIFI6-Touch-LCD-4C | revision v0.x / v1.x |

**Finding your chip revision**, any of:
- Connect with [esptool-js](https://espressif.github.io/esptool-js/) and read
  `Chip is ESP32-P4 (revision vX.Y)`.
- Run `python -m esptool --chip esp32p4 chip_id`.
- Check the first line of the serial log: `ESP-ROM:esp32p4-eco2-…` means a
  v1.x chip.

A wrong-revision image doesn't boot. The serial log shows
`Illegal instruction` right after `entry 0x...`, or `Image requires chip rev`.

### With esptool (recommended)

```bash
pip install --upgrade esptool
python -m esptool --chip esp32p4 -p COM3 erase_flash          # optional; also clears settings
python -m esptool --chip esp32p4 -p COM3 -b 460800 write_flash 0x0 plane-radar-4in-720x720-chip-v1-v1.3.0.bin
```

Replace `COM3` with your port (`/dev/ttyACM0` on Linux,
`/dev/cu.usbmodem…` on macOS). Skip `erase_flash` to keep your settings and
favorites.

### With esptool-js (browser)

1. Open https://espressif.github.io/esptool-js/ in Chrome or Edge and click
   **Connect**.
2. **Change Flash Address from `0x1000` to `0x0`.** This is the most common
   mistake. At the wrong address the board logs `invalid header: 0xffffffff`
   and resets in a loop.
3. Choose the `.bin`, then click **Program** and wait for it to finish.
4. Press **RESET** on the board.

If the board isn't detected, hold **BOOT**, tap **RESET**, then release BOOT.

### Building from source

See the [README](../README.md#building-and-flashing). You need ESP-IDF v5.5.2
or newer.

---

## Troubleshooting

**The serial log helps with everything.** Open a terminal at **115200 baud**,
press RESET, and copy everything from `ESP-ROM:` onward. Each release has
`symbols-*.elf` files for decoding crash backtraces.

| Symptom | Likely cause and fix |
|---|---|
| Black screen, USB keeps disconnecting | Wrong address or wrong chip image. `invalid header: 0xffffffff` → flash at `0x0`. `Illegal instruction` at `entry` → use the other `chip-v1`/`chip-v3` file. |
| "PLANE RADAR – Loading..." then a reboot | A crash during start-up. Send the log; the `abort()` line plus backtrace pinpoints it. |
| Boot screen says "Wi-Fi module not responding" | The P4 can't talk to the ESP32-C6. The radar still runs, but offline. Power-cycle the board. If it persists, the C6's ESP-Hosted firmware may need updating. |
| Log says `major version mismatch — OTA coprocessor` | A recommendation to update the C6 firmware. Older C6 firmware still works with this version. |
| "Wrong password?" or "Network not found" in the status pill | Re-enter the password on the Wi-Fi page. The C6 supports 2.4 GHz networks. |
| "Rate limited - slow down polling" | The feed is asking for fewer requests. Raise **Update every**, or switch **ADS-B feed**. |
| "Loading traffic..." forever | Check the internet connection, or try another feed. |
| No aircraft, but everything else works | Check **Filters** (the status pill says "filtered"). Also zoom out; there may simply be no traffic nearby. |
| Airline or route is wrong | Routes come from adsbdb.com's community list by flight number, and can be stale. Corrections can be reported on [adsbdb's GitHub](https://github.com/mrjackwills/adsbdb/issues). |
| Small airports missing | They appear below about 35 km of zoom. Check **Runways** and **Airport codes** in Display. |
| Animation feels slow | Lower the sweep trail, turn off Smooth trail fade or Trails, or turn the sweep off. |
| Picture is the wrong way round | Set **Display → Screen rotation**, then restart. |
| Clock shows "Waiting for the time..." | It hasn't reached an internet time server yet. Check Wi-Fi; the time is set within a few seconds of connecting. |
| Clock is off by whole hours | Choose your **Time zone** in Clock & alarms. |
| Freezes while rotated (v1.4.0–v1.5.0) | Fixed in v1.5.1; flash it. Your rotation setting is kept. |
| Touch is mirrored or rotated (picture is right) | Report it with your board model and rotation setting. |
| Card says "no history" | The feed doesn't have this aircraft's history (common for aircraft that just appeared, or TIS-B/MLAT-only targets), or its history files are unreachable. Try another **ADS-B feed**. |
