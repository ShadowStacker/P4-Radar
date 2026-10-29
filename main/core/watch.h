#pragma once

#include <string>

#include "core/settings.h"
#include "core/traffic.h"

namespace core {

/**
 * Why an aircraft is on the user's watchlist ("military", "helicopter",
 * "C17", "RCH*"), or "" if it isn't (or the watchlist is off). Types are
 * matched like the type filter; callsigns and registrations with an optional
 * trailing * for a prefix.
 */
std::string watchReason(const Aircraft& a, const Settings& s);

/** True if any watchlist rule is set (and the watchlist is on). */
bool watchActive(const Settings& s);

}  // namespace core
