#pragma once

#include <string>

/**
 * The settings page for phones: a small web server on the local network,
 * at the radar's IP address on port plat::kWebPort. The QR code on the
 * radar's "Phone settings page" opens it.
 */
namespace app::web {

/** UI task, from app::tick(): starts or stops the server with the setting. */
void service();
/** True while the server runs. */
bool running();

/** "http://192.168.1.42:50080" ("" while offline), also in the QR code. */
std::string address();

/**
 * Checks that the server answers by asking it from the radar itself
 * (in the background). selfTest(): 0 = not known yet, 1 = answers,
 * -1 = doesn't answer.
 */
void startSelfTest();
int selfTest();

}  // namespace app::web
