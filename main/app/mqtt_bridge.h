#pragma once

#include <string>
#include <vector>

#include "core/logbook.h"
#include "core/traffic.h"

namespace app::mqtt {

/** Network task, every pass: connects, reconnects or stops as the settings say; discovery after a connect. */
void service();

/** Network task, after each update: the traffic (when it changed) and one event per alert. */
void traffic(const std::vector<core::Aircraft>& shown, const core::UpdateEvents& ev,
             const std::vector<core::logbook::Entry>& firsts, const char* source);

/** For Settings: "Off", "Connecting to ...", "Connected to ...". */
std::string status();

}  // namespace app::mqtt
