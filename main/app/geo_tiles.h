#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "core/geo_shapes.h"

/**
 * The map overlay's cells. Each cell shows its detailed shapes once they are
 * loaded (RAM, then the SD card, then this project's GitHub repository) and
 * the coarse built-in ones until then, or when there's nothing better.
 */
namespace app::geo {

/** UI task: the cells on screen, most important first. Missing detailed ones get fetched. */
void want(const std::vector<int>& cells);

/**
 * UI task: what to draw for a cell (null if there's nothing in it). `detailed`
 * says whether that's the detailed version.
 */
std::shared_ptr<const core::geo::Shapes> shapes(int cell, bool* detailed = nullptr);

/** Goes up whenever a detailed cell arrives: time to redraw. */
uint32_t generation();

/** Network task: fetches one missing cell. `online` = may download. True if it did something. */
bool service(bool online);

/** Whether a cell has a detailed file (exposed for tests). */
bool hasDetail(int cell);

/** Where a cell's detailed file is downloaded from, and kept on the SD card. */
std::string url(int cell);
std::string sdPath(int cell);

}  // namespace app::geo
