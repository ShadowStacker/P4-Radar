#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/traffic.h"

/**
 * Flight replay: the traffic around home recorded every few seconds into one
 * file per day on the SD card (replay/YYYY-MM-DD.rpl), each snapshot a
 * compressed frame, and read back to play the day again on the radar.
 */
namespace core::replay {

constexpr int kFrameEveryS = 15;  // a snapshot this often
constexpr int kKeepDays = 7;      // older day files are deleted
constexpr size_t kHeaderBytes = 16;

struct FrameHeader {
  int64_t time_s = 0;      // unix time of the snapshot
  uint32_t packed = 0;     // compressed bytes after the header
  uint32_t raw = 0;        // their size uncompressed
};

/** A whole frame (header and compressed aircraft) for the file. */
std::string encodeFrame(int64_t time_s, const std::vector<Aircraft>& aircraft);
/** Reads a header; false if it isn't one (a damaged file stops there). */
bool readHeader(const uint8_t* p, FrameHeader* out);
/** The aircraft of a frame from the bytes after its header. */
bool decodeFrame(const FrameHeader& h, const uint8_t* packed, std::vector<Aircraft>* out);

/** "replay/2026-11-01.rpl" for yyyymmdd. */
std::string fileName(int date);
/** The date of such a file name ("2026-11-01.rpl" -> 20261101), 0 if it isn't one. */
int dateOfFile(const std::string& name);

}  // namespace core::replay
