#pragma once

#include <string>

/**
 * One file with everything worth keeping: the settings (with the saved
 * Wi-Fi networks), favorites and silenced aircraft, in the same text forms
 * as settings.txt and favorites.txt. Made and read by the phone page.
 */
namespace core::backup {

struct Parts {
  std::string settings;   // settings.txt text
  std::string favorites;  // favorites.txt text
  std::string silenced;   // same form as favorites.txt
  bool has_settings = false, has_favorites = false, has_silenced = false;
};

/** The backup file. `title` goes in the first line (version and date). */
std::string make(const std::string& title, const Parts& parts);

/** Splits a backup file. False if it isn't one. */
bool split(const std::string& text, Parts* out);

}  // namespace core::backup
