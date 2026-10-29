#pragma once

#include <cstdint>
#include <vector>

#include "core/weather.h"

/**
 * Weather radar downloads (network task): the list of radar frames, then the
 * tiles the UI asks for, decoded and handed back ready to draw.
 */
namespace app::weather {

/** UI task: whether the overlay is on (nothing is downloaded while it's off). */
void setEnabled(bool on);
/** UI task: tile keys (core::weather::key) the screen needs, most important first. */
void request(const std::vector<uint64_t>& missing);
/** UI task: the next decoded tile. The caller owns it (delete). */
bool popReady(uint64_t* key, core::weather::Tile** tile);
/** Times (unix s) of the radar frames available, oldest first; empty until the first download. */
std::vector<int64_t> frames();

/** Network task: one unit of work (frame list or a tile). True if it did something. */
bool service();

}  // namespace app::weather
