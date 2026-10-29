#pragma once

#include <cstdint>

namespace app {

/** UI task, from tick(): tries the next saved network after a while without a connection. */
void serviceWifi(uint32_t now_ms);

}  // namespace app
