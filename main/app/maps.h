#pragma once

#include <cstdint>
#include <vector>

#include "core/tiles.h"

/**
 * Background map tiles: the UI says which tiles it is missing, the network
 * task finds them (RAM cache, then SD card, then the internet), decodes and
 * recolors them, and hands them back ready to draw.
 */
namespace app::maps {

/** How tiles should look; tiles are recolored while decoding. */
struct Look {
  core::tiles::Variant variant = core::tiles::Variant::None;
  bool tint = true;
  uint8_t tint_r = 0, tint_g = 255, tint_b = 128;
  uint8_t brightness = 60;  // %

  bool operator==(const Look& o) const {
    return variant == o.variant && tint == o.tint && tint_r == o.tint_r && tint_g == o.tint_g &&
           tint_b == o.tint_b && brightness == o.brightness;
  }
  bool operator!=(const Look& o) const { return !(*this == o); }
};

/**
 * UI task: the tiles the screen needs but doesn't have, most important
 * first. Replaces the previous request. A new look discards tiles in flight.
 */
void request(const Look& look, const std::vector<uint64_t>& missing);

/**
 * UI task: the next finished tile for the current look: 256x256 RGB565.
 * The caller owns `pixels` and frees them with plat::freeLarge().
 */
bool popReady(uint64_t* key, uint16_t** pixels);

/** Network task: one unit of work. `online` = may download. True if it did something. */
bool service(bool online);

/**
 * CARTO API key in use: Settings::carto_key, or else the first line of
 * carto_key.txt on the SD card. Empty = none (dark streets fall back to OSM).
 */
std::string cartoKey();
bool hasCartoKey();
/** Where the key came from, for the settings page ("settings", "SD card", ""). */
const char* cartoKeySource();

// --- SD card -------------------------------------------------------------------
enum class SdState : uint8_t { Unknown, NoCard, Mounted, Unmounted };

struct SdStatus {
  SdState state = SdState::Unknown;
  uint64_t total_bytes = 0;
  uint64_t free_bytes = 0;
  char error[48] = "";
  // Saved map tiles (after a count).
  bool counted = false;
  bool busy = false;  // counting or clearing
  uint32_t tiles = 0;
  uint64_t tile_bytes = 0;
};

enum class SdAction : uint8_t { Mount, Unmount, Count, Clear };

SdStatus sdStatus();
/** Queues a card action for the network task. */
void sdRequest(SdAction action);
/** Network task: runs a queued card action (and the first mount). True if it did something. */
bool serviceSd();

/** Converts a decoded RGBA tile to RGB565 with the look applied (exposed for tests). */
void recolor(const uint8_t* rgba, int pixels, const Look& look, uint16_t* out);

}  // namespace app::maps
