#pragma once

#include <cstdint>

namespace core {

enum class Units : uint8_t { Km = 0, Mi = 1, Nm = 2 };
enum class LabelMode : uint8_t { Off = 0, Callsign = 1, Full = 2 };
enum class ColorMode : uint8_t { Classic = 0, Altitude = 1 };
enum class Theme : uint8_t { Phosphor = 0, Amber = 1, Ocean = 2, Night = 3 };
enum class DataSource : uint8_t { AdsbFi = 0, AirplanesLive = 1, AdsbLol = 2 };

/**
 * Everything the user can change from the touch menu. Persisted to NVS as a
 * versioned blob (bump kVersion whenever the layout changes) plus separate
 * Wi-Fi credential keys so a firmware update never loses the network.
 */
struct Settings {
  static constexpr uint16_t kVersion = 1;

  // Wi-Fi (stored separately; see platform settings store)
  char wifi_ssid[33] = "";
  char wifi_pass[65] = "";

  // Location
  double home_lat = 52.3676;  // Amsterdam, same default as the original project
  double home_lon = 4.9041;
  float home_radius_km = 25.0f;  // zoom level the Home button returns to
  bool home_set = false;         // false until the user picks a location

  // Display
  Units units = Units::Km;
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
  bool sound_alerts = true;        // emergency squawks + close approach
  float alert_radius_km = 3.0f;
  int32_t alert_max_alt_ft = 5000;

  // Filters
  bool show_ground = false;
  int32_t min_alt_ft = 0;
  int32_t max_alt_ft = 60000;

  // Data
  DataSource source = DataSource::AdsbFi;
  uint8_t poll_interval_s = 5;
};

}  // namespace core
