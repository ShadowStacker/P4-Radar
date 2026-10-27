#pragma once

#include <cstdint>
#include <vector>

namespace core {

enum class Sound : uint8_t {
  Click = 0,     // UI touch feedback
  Ping,          // sonar ping, once per sweep revolution
  NewAircraft,   // soft two-note chime
  Alert,         // close approach: three short beeps
  Emergency,     // emergency squawk: warbling siren
  Startup,       // rising arpeggio at boot
  Home,          // whoosh-ish sweep when returning home
  FavoriteSeen,  // bright rising three-note fanfare
  FavoriteNear,  // insistent two-tone "ding-dong", repeated
  kCount
};

/** Renders a sound effect as mono 16-bit PCM. Pure math, no assets. */
std::vector<int16_t> synthesize(Sound s, int sample_rate);

}  // namespace core
