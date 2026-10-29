#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "core/favorites.h"
#include "core/settings.h"

/**
 * Plain-text copies of the settings and favorites, kept on the SD card
 * (settings.txt and favorites.txt) so they can be edited on a computer and
 * survive anything that wipes the board's flash.
 */
namespace core::settings_text {

constexpr char kSettingsFile[] = "settings.txt";
constexpr char kFavoritesFile[] = "favorites.txt";

/** Every setting as commented "key = value" lines, grouped in sections. */
std::string serialize(const Settings& s);

/**
 * Applies the "key = value" lines in `text` on top of *s (keys that are
 * missing keep their current value; numbers are clamped to their valid
 * range). Unknown keys and bad values are reported in `problems` (may be
 * null) and otherwise ignored. Returns the number of settings read.
 */
int parse(const std::string& text, Settings* s, std::vector<std::string>* problems = nullptr);

/**
 * One favorite per line: "A1B2C3  N123AB  B738" (hex, label, type), with a
 * header. `unresolved` tail numbers (not looked up yet) follow on their own.
 */
std::string serializeFavorites(const std::vector<Favorite>& favs,
                               const std::vector<std::string>& unresolved = {});

/**
 * Reads favorites.txt. Lines that start with a 6-digit ICAO hex become
 * favorites; any other first word is taken as a tail number (registration)
 * to look up online and goes to `lookups`. '#' starts a comment. Duplicates
 * are dropped and the list stops at kMaxFavorites.
 */
void parseFavorites(const std::string& text, std::vector<Favorite>* favs, std::vector<std::string>* lookups);

/**
 * Every setting except the Wi-Fi section, as JSON for the phone page:
 * {"sections":[{"title":..,"fields":[{"key","type","value","help",
 * "min","max","options"}]}]}. Values are in the same text form as the file,
 * so a change goes back through parse() as "key = value".
 */
std::string toJson(const Settings& s);

/** True for a 6-character hex string (an ICAO 24-bit address). */
bool isIcaoHex(const char* s);

}  // namespace core::settings_text
