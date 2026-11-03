#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/sound_synth.h"

/** Custom sounds: WAV files from the SD card's sounds/ folder. */
namespace core::wav {

/**
 * Decodes a WAV file (PCM 8, 16, 24 or 32 bit, or 32-bit float; any number
 * of channels and any sample rate) to mono 16-bit at `rate`, at most
 * `max_seconds` long. False with a reason in *error.
 */
bool decode(const uint8_t* data, size_t size, int rate, double max_seconds, std::vector<int16_t>* out,
            std::string* error);

/** File name for a sound in sounds/, e.g. "aircraft_detected.wav"; nullptr for none. */
const char* fileName(Sound s);
/** Longest clip kept for a sound, in seconds. */
double maxSeconds(Sound s);

}  // namespace core::wav
