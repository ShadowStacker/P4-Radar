#pragma once

#include <string>
#include <vector>

/**
 * The "What's new" notes shown after an update, read from CHANGELOG.md
 * (built into the firmware).
 */
namespace core::release_notes {

struct Section {
  std::string version;  // "v1.20.0"
  std::string date;     // "2026-10-31"
  std::vector<std::string> items;  // brief: each bullet's title and first sentence, plain text
};

/** The changelog's "## [vX.Y.Z] - date" sections, newest first (as in the file). */
std::vector<Section> parse(const std::string& markdown);

/**
 * What's new on `current` since `previous` ("1.18.3", with or without the
 * v): the sections after `previous` up to `current`, newest first, at most
 * `max`. An empty `previous` gives just `current`.
 */
std::vector<Section> newSince(const std::vector<Section>& all, const std::string& current, const std::string& previous,
                              int max);

/** Plain text the screen's fonts can show: markdown marks removed, unusual characters replaced. */
std::string plain(const std::string& markdown);

}  // namespace core::release_notes
