#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

/** Checking GitHub for a newer firmware release (parsing only; no I/O). */
namespace core::update {

/** The project's latest published release (GitHub REST API). */
constexpr char kLatestReleaseUrl[] = "https://api.github.com/repos/ShadowStacker/P4-Radar/releases/latest";

/** "v1.12.1" or "1.12.1" -> {1, 12, 1}. False if it isn't a version. */
bool parseVersion(const char* text, int out[3]);
/** <0, 0 or >0 like strcmp; versions that don't parse sort lowest. */
int compareVersions(const char* a, const char* b);

struct Release {
  std::string tag;        // "v1.13.0"
  std::string asset_url;  // the update image for this board, or "" if the release has none
  size_t asset_size = 0;
};

/**
 * Name of the update image (app only, not the USB image) for a board
 * variant like "4in-720x720-chip-v1": "plane-radar-4in-720x720-chip-v1-v1.13.0-ota.bin".
 */
std::string assetName(const char* variant, const char* tag);

/** Reads the releases/latest JSON and picks this variant's update image. */
bool parseLatestRelease(const char* json, size_t len, const char* variant, Release* out);

}  // namespace core::update
