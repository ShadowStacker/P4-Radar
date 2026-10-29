#pragma once

#include <string>

/**
 * The settings page for phones: a small web server on the local network
 * (http://plane-radar.local, or the radar's IP address), protected by a
 * 4-digit PIN shown on the radar. The QR code on the radar's "Phone
 * settings page" opens it already logged in.
 */
namespace app::web {

/** UI task, from app::tick(): starts or stops the server with the setting; makes up a PIN the first time. */
void service();
/** True while the server runs. */
bool running();

/** "http://192.168.1.42" ("" while offline). */
std::string address();
/** The address with the PIN, for the QR code: "http://192.168.1.42/?pin=1234". */
std::string loginUrl();
/** The page's name on the network. */
constexpr char kHostName[] = "plane-radar.local";

/**
 * Checks that the server answers by asking it from the radar itself
 * (in the background). selfTest(): 0 = not known yet, 1 = answers,
 * -1 = doesn't answer.
 */
void startSelfTest();
int selfTest();

/** UI task: a new PIN; phones that were logged in have to enter it again. */
void newPin();

}  // namespace app::web
