#pragma once

#include <cstdint>

#include "core/clock.h"

namespace core {

enum class Units : uint8_t { Km = 0, Mi = 1, Nm = 2 };
enum class LabelMode : uint8_t { Off = 0, Callsign = 1, Full = 2 };
enum class ColorMode : uint8_t { Classic = 0, Altitude = 1 };
enum class Theme : uint8_t { Phosphor = 0, Amber = 1, Ocean = 2, Night = 3, Custom = 4 };
enum class DataSource : uint8_t { AdsbFi = 0, AirplanesLive = 1, AdsbLol = 2 };

/** A remembered Wi-Fi network besides the current one (Settings::wifi_ssid). */
struct SavedWifi {
  char ssid[33] = "";
  char pass[65] = "";
};
constexpr int kMaxSavedWifi = 4;  // plus the current one: five networks in all

/**
 * Everything the user can change from the touch menu. Persisted to NVS as a
 * versioned blob plus separate Wi-Fi credential keys, so a firmware update
 * never loses the network.
 */
struct Settings {
  // New fields are appended (see the end of the struct). The version marks
  // where a saved blob's fields end; bump it whenever fields are added.
  //   1: up to poll_interval_s (v1.0.x) or autohide_buttons (v1.1.0)
  //   2: up to favorite_near_radius_km (v1.2.0)
  //   3: up to type_filter (v1.3.0)
  //   4: up to extended_trail (v1.4.0)
  //   5: up to show_route_info (v1.5.0)
  //   6: up to alarms (v1.6.0)
  //   7: up to custom_alt_high_hue (v1.7.0)
  //   8: up to sd_cache (v1.8.0)
  //   9: up to wake_on_alarm (v1.9.1)
  //  10: up to carto_key (v1.10.0); rotation re-based (see below)
  //  11: up to wifi_saved (v1.15.0)
  //  12: up to auto_update_check (v1.16.0)
  static constexpr uint16_t kVersion = 12;

  // Wi-Fi: the network to use first (stored separately too; see the platform
  // settings store). Other saved networks are in wifi_saved.
  char wifi_ssid[33] = "";
  char wifi_pass[65] = "";

  // Location
  double home_lat = 44.2541;  // Roberts Field (RDM), Redmond, Oregon
  double home_lon = -121.1500;
  float home_radius_km = 25.0f;  // zoom level the Home button returns to
  bool home_set = false;         // false until the user picks a location

  // Display
  Units units = Units::Mi;
  Theme theme = Theme::Phosphor;
  uint8_t brightness = 80;  // %
  LabelMode labels = LabelMode::Full;
  ColorMode color_mode = ColorMode::Altitude;
  bool show_trails = true;
  bool show_runways = true;
  bool show_airport_labels = true;
  bool show_rings = true;
  bool show_speed_vectors = true;
  bool sweep_enabled = true;
  uint8_t sweep_rpm = 15;          // revolutions per minute
  bool sweep_reveal = false;       // aircraft fade out until the beam passes
  uint8_t dim_after_min = 0;       // 0 = never
  uint8_t dim_brightness = 10;     // % while dimmed

  // Sound
  bool sound_enabled = true;
  uint8_t volume = 60;             // %
  bool sound_touch = true;
  bool sound_new_aircraft = true;
  bool sound_sweep_ping = false;
  bool sound_alerts = true;        // emergency squawks (proximity has its own switch below)
  float alert_radius_km = 3.0f;
  int32_t alert_max_alt_ft = 5000;

  // Filters
  bool show_ground = false;
  int32_t min_alt_ft = 0;
  int32_t max_alt_ft = 60000;

  // Data
  DataSource source = DataSource::AdsbFi;
  uint8_t poll_interval_s = 5;

  // ---- Added after v1.0.3. Only append below this line: saved settings from
  // older firmware are loaded by copying the prefix they contain, and anything
  // past it keeps the defaults here.
  bool military_only = false;
  bool autohide_buttons = true;  // fade the radar's buttons after 10 s idle

  // v1.2.0
  bool sound_proximity = false;        // any aircraft inside alert_radius_km
  bool sound_favorite_seen = true;     // a favorite comes into range
  bool sound_favorite_near = true;     // a favorite comes within favorite_near_radius_km
  float favorite_near_radius_km = 15.0f;

  // v1.3.0
  uint8_t sweep_trail_deg = 70;     // length of the fading trail behind the beam; 0 = clean bar
  bool sweep_smooth = false;        // finer gradient (more slices) for the trail
  uint8_t category_mask = 0xFF;     // core::CategoryBit flags of aircraft kinds to show
  uint16_t min_speed_kt = 0;
  uint16_t max_speed_kt = 1000;     // >= kSpeedNoLimit means no upper limit
  char type_filter[24] = "";        // e.g. "B738, A32*, C17" (empty = all types)

  // v1.4.0
  // Screen orientation in quarter turns clockwise (0-3), applied at boot.
  // 0 is the panel turned 180 deg (core::panelQuarterTurns()); before v1.10
  // 0 was the panel's native orientation, and saved values are converted.
  uint8_t rotation = 0;
  bool extended_trail = true;       // download the flight history of the tapped aircraft

  // v1.5.0: Custom theme colors (hue 0-359, saturation/brightness in %).
  uint16_t custom_hue = 175;              // radar color: rings, sweep, accents
  uint8_t custom_sat = 80;
  uint8_t custom_bg = 6;                  // background brightness
  uint8_t custom_grid = 45;               // rings and compass brightness
  uint16_t custom_aircraft_hue = 0;       // aircraft in "Theme" color mode
  uint8_t custom_aircraft_sat = 0;        // 0 = white
  uint16_t custom_label_hue = 0;
  uint8_t custom_label_sat = 0;           // 0 = white
  // v1.5.0: sizes, apply to every theme
  uint8_t aircraft_scale = 100;           // % of the standard aircraft symbol size
  uint8_t label_size = 1;                 // 0 small, 1 normal, 2 large
  uint8_t airport_codes = 0;              // 0 ICAO (local codes like S39 fall back to IATA), 1 IATA
  bool show_route_info = true;            // look up airline and route of the tapped aircraft

  // v1.6.0: clock
  bool clock_mode = false;                // clock face (numerals + hands) instead of compass headings
  bool clock_24h = false;
  bool clock_seconds = true;              // seconds hand
  uint8_t time_zone = 1;                  // index into core::timeZone(); 1 = US Pacific
  Alarm alarms[kMaxAlarms];

  // v1.7.0
  bool clock_smooth_seconds = false;      // sweeping instead of ticking seconds hand
  bool clock_show_date = true;            // date window at three o'clock
  uint16_t clock_hand_hue = 0;            // hour and minute hands
  uint8_t clock_hand_sat = 0;             // 0 = white
  uint16_t clock_second_hue = 0;          // seconds hand
  uint8_t clock_second_sat = 85;          // red
  // Custom theme altitude colors: hue at the ground, 10,000 ft and 40,000 ft
  // (the defaults match the standard orange-green-blue-magenta scale).
  uint16_t custom_alt_low_hue = 20;
  uint16_t custom_alt_mid_hue = 140;
  uint16_t custom_alt_high_hue = 300;

  // v1.8.0: background map (core::tiles::Style) and SD card
  uint8_t map_style = 0;                  // core::tiles::Style: 0 off, 1 dark streets, 2 topo, 3 FAA, 4 satellite, 5 street map
  bool map_labels = true;                 // place names on the dark street map
  bool map_tint = true;                   // recolor the map in the theme color
  uint8_t map_brightness = 60;            // %
  bool sd_cache = true;                   // keep downloaded map tiles on the SD card

  // v1.9.1
  bool wake_on_alarm = false;             // alarms ring and wake a sleeping screen (else only a tap wakes it)

  // v1.10.0
  char carto_key[64] = "";                // CARTO basemaps API key (dark street map); empty = use OSM

  // v1.15.0: more Wi-Fi networks, tried in turn when the current one can't
  // connect (empty ssid = unused slot)
  SavedWifi wifi_saved[kMaxSavedWifi];

  // v1.16.0
  bool auto_update_check = true;          // look for new firmware an hour apart
};

}  // namespace core
