#pragma once

#include <cstdint>
#include <string>

#include "core/geo.h"

namespace core::tiles {

/**
 * Web map tiles (256 px, Web Mercator, z/x/y) for the background map.
 * Variant = what is actually downloaded: a style plus its options.
 */
enum class Variant : uint8_t { None = 0, DarkLabels, DarkNoLabels, Topo, OsmStandard, FaaSectional, kCount };

constexpr int kTileSize = 256;

/** Map styles offered in Settings (Display -> Background map). */
enum class Style : uint8_t { Off = 0, DarkStreets = 1, Topo = 2, FaaSectional = 3 };

/**
 * The dark street map comes from CARTO, which needs a (free) API key; without
 * one, OpenStreetMap's standard map is used and darkened instead.
 */
Variant variantFor(Style style, bool labels, bool have_carto_key);
int maxZoom(Variant v);
/** Coarsest zoom the source has; wider views fetch these tiles and shrink them. */
int minZoom(Variant v);
/** Short directory name on the SD card (8.3-safe). */
const char* dirName(Variant v);
/** Credit line that must be shown with the map. */
const char* attribution(Variant v);
/** True for light maps (topo, OSM): tinting inverts them so they stay dark. */
bool isLight(Variant v);
/** Charts keep their colors: the theme tint would hide what they show. */
bool keepColors(Variant v);

/** Tile download URLs, best first (the second is a fallback path). `key` = CARTO API key. */
std::string url(Variant v, int z, uint32_t x, uint32_t y, int alternative = 0, const char* key = "");
/** "<root>/tiles/<dir>/<z>/<x>/<y>.png" */
std::string sdPath(const std::string& root, Variant v, int z, uint32_t x, uint32_t y);

/** Packs a tile id into one number (for caches and queues). */
uint64_t key(Variant v, int z, uint32_t x, uint32_t y);
void unpack(uint64_t key, Variant* v, int* z, uint32_t* x, uint32_t* y);
/** Key of the tile `levels` zoom levels up that contains this one. */
uint64_t parentKey(uint64_t key, int levels);

/**
 * Zoom level whose tiles come closest to 1 screen pixel per tile pixel at
 * this scale and latitude, clamped to what the variant offers.
 */
int zoomFor(Variant v, double km_per_px, double lat);

/** Tile column/row containing a point, and a tile's edges in degrees. */
uint32_t tileX(double lon, int z);
uint32_t tileY(double lat, int z);
double tileLon(uint32_t x, int z);  // west edge of column x (x = 2^z gives the east edge)
double tileLat(double y, int z);    // north edge of row y; fractional rows allowed

/**
 * RGBA tile pixels -> RGB565 for drawing. With `tint` the map becomes one
 * color: brightness from the pixel's luminance (inverted for light maps so
 * they stay dark), contrast lifted so roads and labels stand out. Without
 * it the original colors are just dimmed. `brightness` is 0-100 %.
 */
void recolor(const uint8_t* rgba, int pixels, bool light_map, bool tint, uint8_t tr, uint8_t tg, uint8_t tb,
             int brightness, uint16_t* out);

}  // namespace core::tiles
