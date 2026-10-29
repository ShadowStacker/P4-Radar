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
 * The radar's own Wi-Fi network, for phones that can't reach it through the
 * router. Turns itself off after 30 minutes without a phone visit (and with
 * a restart). UI task.
 */
bool hotspotStart();
void hotspotStop();
bool hotspotOn();
std::string hotspotSsid();
std::string hotspotPassword();
/** For a QR code that joins the network: "WIFI:T:WPA;S:<ssid>;P:<password>;;". */
std::string hotspotJoinCode();
/** "http://192.168.4.1:50080" */
std::string hotspotAddress();

/**
 * Checks that the server answers by asking it from the radar itself
 * (in the background). selfTest(): 0 = not known yet, 1 = answers,
 * -1 = doesn't answer.
 */
void startSelfTest();
int selfTest();

}  // namespace app::web
