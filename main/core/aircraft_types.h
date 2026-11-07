#pragma once

#include <cstdint>

/**
 * Which icon an aircraft is drawn with: from its ICAO type designator
 * ("A320", "B738", "C172", "H60") when it's known, otherwise from its ADS-B
 * emitter category (A1 light ... A7 rotorcraft, B1 glider, C1 vehicle).
 * Icons by ADS-B Radar - https://adsb-radar.com.
 */
namespace core::aircraft {

enum class Icon : uint8_t {
  // By emitter category (when the type isn't in the table).
  Generic,     // A0 / unknown
  Light,       // A1
  Small,       // A2
  Large,       // A3
  HighVortex,  // A4 (757 class)
  Heavy,       // A5
  HighPerf,    // A6
  Rotor,       // A7
  Drone,       // B6
  Glider,      // B1
  Airship,     // B2 lighter than air
  Parachute,   // B3
  Ultralight,  // B4
  Ground,      // C1-C7 surface vehicles
  // By type.
  A320, A330, A340, A380, B737, B747, B767, B777, B787, MD11,
  RearJet,     // MD-80/90, F100, 717, 727, BAe 146
  CRJ, ERJ, E195,
  Turboprop,   // Dash 8, ATR, Saab 340
  TwinProp,    // King Air, Baron, light twins
  SingleProp,  // Cessna, Piper, Cirrus
  BizJet,      // Citation, Learjet, Phenom
  BigBizJet,   // Gulfstream, Global, Challenger
  Falcon,      // Dassault trijets/twins
  C130,        // military turboprop transports
  Freighter,   // C-17, C-5, An-124, A400M
  Fighter,
  Tiltrotor,
  Amphibian,
  kCount
};

/** The SVG/mask name for an icon ("a320", "cessna", ...). */
const char* iconName(Icon icon);
/** Edge length at the standard size, px on the 800 px screen: light ones small, jumbos big. */
int baseSize(Icon icon);

/** The icon for a type designator (case doesn't matter; "" if unknown) and emitter category ("A3"...). */
Icon iconFor(const char* type, const char* category);

/** The icon for a type designator alone; false if the type isn't in the table. */
bool iconForType(const char* type, Icon* out);

}  // namespace core::aircraft
