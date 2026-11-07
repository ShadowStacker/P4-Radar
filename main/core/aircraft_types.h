#pragma once

#include <cstdint>

/**
 * Which icon an aircraft is drawn with: from its ICAO type designator
 * ("A320", "B738", "C172", "H60") when it's known, otherwise from its ADS-B
 * emitter category (A1 light ... A7 rotorcraft, B1 glider, C1 vehicle).
 * Icons by ADS-B Radar - https://adsb-radar.com - plus some drawn in their
 * style (scripts/aircraft_families.py) for propeller aircraft, helicopters
 * and military types.
 */
namespace core::aircraft {

enum class Icon : uint8_t {
  Generic,  // A0 / unknown
  Light,  // A1 light (unknown type): a high-wing single
  Small,  // A2 small
  Large,  // A3 large
  HighVortex,  // A4 (757 class)
  Heavy,  // A5 heavy
  HighPerf,  // A6 high performance
  Rotor,  // A7 rotorcraft (unknown type)
  Drone,  // B6 unmanned
  Glider,  // B1 glider
  Airship,  // B2 lighter than air
  Parachute,  // B3 parachutist
  Ultralight,  // B4 ultralight
  Ground,  // C1-C7 surface vehicles
  A320,  // A320 family, A220, C919
  A330,  // A300/310/330, A350
  A340,  // A340
  A380,  // A380
  B737,  // 737 family, P-8
  B747,  // 747
  B767,  // 757, 767, KC-46
  B777,  // 777
  B787,  // 787
  MD11,  // MD-11, DC-10, KC-10, L-1011
  RearJet,  // rear-engine jets: MD-80/90, F100, 717, 727, DC-9, BAe 146
  CRJ,  // CRJ
  ERJ,  // ERJ, Legacy 600, Do 328JET
  E195,  // E-Jets, Superjet, ARJ21
  Turboprop,  // regional turboprops: Dash 8, ATR, Saab, Do 328
  TwinProp,  // King Air
  BizJet,  // Citation, Learjet, Phenom, Hawker
  BigBizJet,  // Gulfstream, Global, Challenger
  Falcon,  // Dassault Falcon
  C130,  // four-turboprop transports: C-130, A400M, P-3
  Freighter,  // heavy jet freighters: C-17, C-5, An-124, Il-76
  Tiltrotor,  // V-22, AW609
  Amphibian,  // water bombers and floatplanes
  SingleHigh,  // Cessna 150-210 and other high-wing singles
  SingleLow,  // Piper PA-28/32, Cirrus, Diamond, Bonanza, Mooney, RV
  TurbopropSingle,  // PC-12, TBM, PA-46, T-6, Super Tucano, Air Tractor
  TurbopropSingleHigh,  // Caravan, Kodiak, Beaver, Otter, Porter
  TwinPiston,  // Baron, Seneca, Seminole, Cessna 310-421, DA42
  TwinTurboprop,  // Beech 99, Metro, Jetstream, Conquest, MU-2, Cheyenne
  TwinHigh,  // Twin Otter, Islander, Do 228, Let 410, CASA 212, Skyvan, P68
  B707,  // 707, KC-135, E-6
  Awacs,  // AWACS: E-3, A-50, E-767
  Hawkeye,  // E-2 Hawkeye, C-2
  B52,  // B-52
  B1,  // B-1
  B2,  // B-2
  Reaper,  // MQ-9, MQ-1, Global Hawk, Heron
  FighterSingle,  // single-fin fighters: F-16, F-4, F-5, Tornado, A-4
  FighterTwin,  // twin-fin fighters: F-15, F/A-18, F-22, F-35, F-14, Su-27 family, MiG-29
  FighterDelta,  // delta fighters: Eurofighter, Rafale, Gripen, Mirage 2000, J-10
  Attack,  // A-10
  JetTrainer,  // jet trainers: T-38, Hawk, L-39, M-346, Alpha Jet, T-45, T-7
  Heli2,  // two-blade helicopters: Robinson, Bell 206/505/212/222, UH-1
  Heli3,  // three-blade helicopters: AS350/H125, AS355, EC130, Gazelle
  Heli4,  // four-blade helicopters: H135, H145, Bell 407/429/412, A109, S-76, Dauphin
  Heli5,  // five-blade helicopters: AW139, H160, H175, H225, Mi-8/17, S-61, AW101
  HeliLarge,  // big four-blade helicopters: UH-60, S-92, NH90, Super Puma, CH-53
  HeliTandem,  // tandem rotors: CH-47, CH-46
  HeliAttack,  // attack helicopters: AH-64, AH-1, Mi-24/28, Tiger
  HeliCoax,  // coaxial rotors: Kamov
  kCount
};

/** The mask (SVG file) name for an icon ("a320", "g_heli_4", ...). Several icons may share one. */
const char* iconName(Icon icon);
/** Edge length at the standard size, px on the 800 px screen: light ones small, jumbos big. */
int baseSize(Icon icon);

/** The icon for a type designator (case doesn't matter; "" if unknown) and emitter category ("A3"...). */
Icon iconFor(const char* type, const char* category);

/** The icon for a type designator alone; false if the type isn't in the table. */
bool iconForType(const char* type, Icon* out);

}  // namespace core::aircraft
