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

enum class Icon : uint16_t {
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
  Ground,  // C1-C7 surface vehicles; ground stations
  Turboprop,  // regional turboprops: Dash 8, ATR, Saab, Do 328
  Freighter,  // heavy jet freighters: C-17, C-5, An-124, Il-76
  Tiltrotor,  // V-22, AW609
  SingleHigh,  // Cessna 150-210 and other high-wing singles
  SingleLow,  // Piper PA-28/32, Cirrus, Diamond, Bonanza, Mooney, RV
  TurbopropSingle,  // PC-12, TBM, PA-46, T-6, Super Tucano, Air Tractor
  TwinPiston,  // Baron, Seneca, Seminole, Cessna 310-421, DA42
  TwinTurboprop,  // Beech 99, Metro, Jetstream, Conquest, MU-2, Cheyenne
  TwinHigh,  // Twin Otter, Islander, Do 228, Let 410, CASA 212, Skyvan, P68
  Awacs,  // AWACS: E-3, A-50, E-767
  Hawkeye,  // E-2 Hawkeye, C-2
  B52,  // B-52
  B1,  // B-1
  B2,  // B-2
  FighterSingle,  // single-fin fighters: F-16, F-4, F-5, Tornado, A-4
  FighterTwin,  // twin-fin fighters: F-15, F/A-18, F-22, F-35, F-14, Su-27 family, MiG-29
  FighterDelta,  // delta fighters: Eurofighter, Rafale, Gripen, Mirage 2000, J-10
  JetTrainer,  // jet trainers: T-38, Hawk, L-39, M-346, Alpha Jet, T-45, T-7
  Heli4,  // four-blade helicopters: H135, H145, Bell 407/429/412, A109, S-76, Dauphin
  X_A318,  // Airbus A318
  X_A319,  // Airbus A319
  X_A321,  // Airbus A321
  X_A220_100,  // Airbus A220-100
  X_A220_300,  // Airbus A220-300
  X_C919,  // COMAC C919
  X_B732,  // Boeing 737-200
  X_B733,  // Boeing 737-300
  X_B734,  // Boeing 737-400
  X_B735,  // Boeing 737-500
  X_B736,  // Boeing 737-600
  X_B737,  // Boeing 737-700 / MAX 7
  X_B739,  // Boeing 737-900 / MAX 9 / MAX 10
  X_B752,  // Boeing 757-200
  X_B753,  // Boeing 757-300
  X_B762,  // Boeing 767-200
  X_B764,  // Boeing 767-400
  X_B772,  // Boeing 777-200
  X_B773,  // Boeing 777-300
  X_B77W,  // Boeing 777-300ER
  X_B779,  // Boeing 777X
  X_B788,  // Boeing 787-8
  X_B78X,  // Boeing 787-10
  X_A300,  // Airbus A300
  X_A310,  // Airbus A310
  X_A332,  // Airbus A330-200
  X_A339,  // Airbus A330neo
  X_A359,  // Airbus A350-900
  X_A35K,  // Airbus A350-1000
  X_A346,  // Airbus A340-500/600
  X_B748,  // Boeing 747-8
  X_B74S,  // Boeing 747SP
  X_BELUGA,  // Airbus Beluga
  X_BELUGAXL,  // Airbus BelugaXL
  X_BLCF,  // Boeing Dreamlifter
  X_DC10,  // McDonnell Douglas DC-10 / KC-10
  X_L1011,  // Lockheed L-1011 TriStar
  X_B712,  // Boeing 717
  X_MD80,  // McDonnell Douglas MD-80
  X_MD90,  // McDonnell Douglas MD-90
  X_DC9,  // Douglas DC-9
  X_F100,  // Fokker 100
  X_F70,  // Fokker 70
  X_B722,  // Boeing 727-200
  X_B721,  // Boeing 727-100
  X_BAE146,  // BAe 146 / Avro RJ85
  X_BAE146_300,  // BAe 146-300 / Avro RJ100
  X_T154,  // Tupolev Tu-154
  X_IL62,  // Ilyushin Il-62
  X_YK42,  // Yakovlev Yak-42
  X_CRJ2,  // Bombardier CRJ100/200
  X_CRJ7,  // Bombardier CRJ700
  X_E170,  // Embraer 170
  X_E175,  // Embraer 175
  X_E190,  // Embraer 190
  X_E290,  // Embraer E190-E2
  X_E135,  // Embraer ERJ 135 / Legacy
  X_SU95,  // Sukhoi Superjet 100
  X_ARJ21,  // COMAC ARJ21
  X_DH8C,  // Dash 8-300
  X_DH8D,  // Dash 8 Q400
  X_ATR42,  // ATR 42
  X_ATR72,  // ATR 72
  X_SF34,  // Saab 340
  X_SB20,  // Saab 2000
  X_D328,  // Dornier 328
  X_J328,  // Dornier 328JET
  X_E120,  // Embraer Brasilia
  X_JS41,  // Jetstream 41
  X_F50,  // Fokker 50 / F27
  X_DHC7,  // Dash 7
  X_AN26,  // Antonov An-24/26/32, MA60
  X_C295,  // CASA C-295
  X_CN235,  // CASA CN-235
  X_C510,  // Citation Mustang
  X_C525,  // CitationJet CJ1/M2
  X_C25A,  // Citation CJ2
  X_C25B,  // Citation CJ3
  X_C25C,  // Citation CJ4
  X_C500,  // Citation I/II/Bravo
  X_C560,  // Citation V/Ultra/Encore
  X_C56X,  // Citation Excel/XLS
  X_C680,  // Citation Sovereign
  X_C68A,  // Citation Latitude
  X_C700,  // Citation Longitude
  X_C750,  // Citation X
  X_LJ35,  // Learjet 31/35/36
  X_LJ45,  // Learjet 40/45/70/75
  X_LJ60,  // Learjet 55/60
  X_E50P,  // Phenom 100
  X_E55P,  // Phenom 300
  X_E545,  // Praetor 500 / Legacy 450
  X_E550,  // Praetor 600 / Legacy 500
  X_HDJT,  // HondaJet
  X_SF50,  // Cirrus Vision Jet
  X_PC24,  // Pilatus PC-24
  X_BE40,  // Beechjet 400 / Hawker 400
  X_PRM1,  // Beech Premier
  X_H25B,  // Hawker 800/900
  X_G280,  // Gulfstream G280
  X_GLF4,  // Gulfstream IV/G450
  X_GLF6,  // Gulfstream G650
  X_GA7C,  // Gulfstream G700/G800/G500/G600
  X_GLEX,  // Bombardier Global Express/6000
  X_GL5T,  // Bombardier Global 5000/5500
  X_GL7T,  // Bombardier Global 7500/8000
  X_CL30,  // Challenger 300/350
  X_CL60,  // Challenger 600/650
  X_FA8X,  // Falcon 8X
  X_F2TH,  // Falcon 2000
  X_FA50,  // Falcon 50/900
  X_FA6X,  // Falcon 6X
  X_FA20,  // Falcon 10/20
  X_C152,  // Cessna 150/152
  X_C172,  // Cessna 172
  X_C182,  // Cessna 182/206
  X_C210,  // Cessna 210
  X_C177,  // Cessna Cardinal
  X_C180,  // Cessna 180/185/170 (taildragger)
  X_PA18,  // Piper Super Cub
  X_J3,  // Piper J-3 Cub
  X_BL8,  // Citabria / Decathlon
  X_HUSK,  // Aviat Husky
  X_M7,  // Maule
  X_GA8,  // GippsAero Airvan
  X_PA32,  // Piper Saratoga/Cherokee Six
  X_PA38,  // Piper Tomahawk
  X_PA24,  // Piper Comanche
  X_PA46,  // Piper Malibu/Meridian/M500-M700
  X_SR22,  // Cirrus SR20/SR22
  X_DA40,  // Diamond DA40
  X_DA20,  // Diamond DA20
  X_BE35,  // Beech Bonanza V-tail
  X_BE36,  // Beech Bonanza A36
  X_M20,  // Mooney M20
  X_RV7,  // Van's RV-6/7/8/9/14
  X_RV10,  // Van's RV-10
  X_RV12,  // Van's RV-12, Tecnam Sierra, JMB VL3
  X_LNC4,  // Lancair IV / Evolution
  X_PA28,  // Piper PA-28 Cherokee/Warrior/Archer/Arrow
  X_TB20,  // Socata Trinidad/Tobago
  X_DR40,  // Robin DR400
  X_G115,  // Grob 115/120
  X_AA5,  // Grumman Tiger/Traveler, Globe Swift
  X_TBM9,  // Daher TBM 700/850/900, Epic
  X_KODI,  // Quest Kodiak
  X_PC6,  // Pilatus Porter
  X_DHC2,  // DHC-2 Beaver
  X_DHC3,  // DHC-3 Otter
  X_C208,  // Cessna Caravan
  X_PC12,  // Pilatus PC-12
  X_AT8T,  // Air Tractor (crop duster)
  X_PA25,  // Piper Pawnee
  X_PA23,  // Piper Aztec/Apache
  X_PA34,  // Piper Seneca
  X_PA44,  // Piper Seminole
  X_BE58,  // Beech Baron
  X_BE76,  // Beech Duchess
  X_C310,  // Cessna 310/320
  X_C340,  // Cessna 340/335
  X_C414,  // Cessna 401/402/414
  X_C421,  // Cessna 421/404
  X_DA42,  // Diamond DA42
  X_DA62,  // Diamond DA62
  X_P68,  // Vulcanair/Partenavia P68
  X_PA31,  // Piper Navajo/Chieftain
  X_AEST,  // Piper Aerostar
  X_B350,  // King Air 350
  X_BE9L,  // King Air 90/100
  X_SW4,  // Fairchild Metro/Merlin
  X_JS31,  // Jetstream 31/32
  X_MU2,  // Mitsubishi MU-2
  X_C441,  // Cessna Conquest
  X_PAY3,  // Piper Cheyenne
  X_P180,  // Piaggio Avanti
  X_C337,  // Cessna Skymaster (push-pull)
  X_L410,  // Let 410
  X_BN2,  // Britten-Norman Islander
  X_D228,  // Dornier 228
  X_C212,  // CASA C-212
  X_SC7,  // Shorts Skyvan / 330 / 360
  X_DC3,  // Douglas DC-3 / C-47
  X_DC4,  // Douglas DC-4 / C-54
  X_DC6,  // Douglas DC-6/DC-7
  X_C46,  // Curtiss C-46 Commando
  X_CONI,  // Lockheed Constellation
  X_L188,  // Lockheed Electra
  X_VISC,  // Vickers Viscount
  X_JU52,  // Junkers Ju 52 (trimotor)
  X_AN2,  // Antonov An-2 (biplane)
  X_BE18,  // Beech 18 (twin tail)
  X_BE17,  // Beech Staggerwing (biplane)
  X_G21,  // Grumman Goose
  X_PBY,  // Consolidated Catalina
  X_DHC6,  // DHC-6 Twin Otter
  X_P51,  // P-51 Mustang
  X_SPIT,  // Supermarine Spitfire
  X_HURI,  // Hawker Hurricane
  X_ME09,  // Messerschmitt Bf 109
  X_FW90,  // Focke-Wulf Fw 190
  X_ZERO,  // Mitsubishi Zero
  X_CORS,  // Vought F4U Corsair
  X_P47,  // P-47 Thunderbolt
  X_P40,  // P-40 Warhawk
  X_P38,  // P-38 Lightning (twin boom)
  X_B17,  // B-17 Flying Fortress
  X_B24,  // B-24 Liberator
  X_B25,  // B-25 Mitchell
  X_B29,  // B-29 Superfortress
  X_LANC,  // Avro Lancaster
  X_MOSQ,  // de Havilland Mosquito
  X_T6,  // North American T-6 Texan/Harvard
  X_T28,  // North American T-28 Trojan
  X_STEARMAN,  // Boeing Stearman (biplane)
  X_DH82,  // de Havilland Tiger Moth (biplane)
  X_PITTS,  // Pitts Special (biplane)
  X_YK52,  // Yak-52 / Yak-18
  X_A1SKY,  // Douglas A-1 Skyraider
  X_LGEZ,  // Rutan Long-EZ / VariEze / Cozy
  X_VELO,  // Velocity
  X_E300,  // Extra 300/330
  X_SU26,  // Sukhoi Su-26/29/31
  X_CP23,  // CAP 10/232
  X_KITFOX,  // Kitfox / Zenith / light sport high wing
  X_XCUB,  // CubCrafters Carbon/XCub
  X_GLASAIR,  // Glasair
  X_LA4,  // Lake Buccaneer (pusher amphibian)
  X_ICONA5,  // ICON A5 / SeaRey
  X_F86,  // F-86 Sabre
  X_MG15,  // MiG-15/17
  X_MG21,  // MiG-21
  X_F104,  // F-104 Starfighter
  X_VAMP,  // de Havilland Vampire (twin boom)
  X_HUNT,  // Hawker Hunter
  X_L39,  // Aero L-39/L-159
  X_A4,  // A-4 Skyhawk
  X_F4,  // F-4 Phantom II
  X_F14,  // F-14 Tomcat
  X_F15,  // F-15 Eagle
  X_F18,  // F/A-18 Hornet / Super Hornet
  X_F22,  // F-22 Raptor
  X_F35,  // F-35 Lightning II
  X_SU27,  // Sukhoi Su-27/30/35
  X_MG29,  // MiG-29
  X_TORNADO,  // Panavia Tornado
  X_JAGUAR,  // SEPECAT Jaguar
  X_MIR2,  // Mirage 2000 / Mirage III
  X_DRAKEN,  // Saab Draken
  X_VIGGEN,  // Saab Viggen
  X_RAFALE,  // Dassault Rafale
  X_GRIPEN,  // Saab Gripen
  X_EUFI,  // Eurofighter Typhoon
  X_J10,  // Chengdu J-10
  X_F16,  // F-16 Fighting Falcon
  X_F5,  // F-5 / T-38
  X_HAWK,  // BAE Hawk / T-45
  X_AJET,  // Dassault Alpha Jet
  X_M346,  // M-346 / Yak-130
  X_T7,  // Boeing T-7 Red Hawk
  X_A10,  // A-10 Thunderbolt II
  X_AV8B,  // Harrier
  X_F117,  // F-117 Nighthawk
  X_SR71,  // SR-71 Blackbird
  X_U2,  // Lockheed U-2
  X_C5,  // C-5 Galaxy
  X_A400,  // Airbus A400M
  X_P3,  // P-3 Orion / L-188
  X_C160,  // Transall C-160
  X_AN124,  // Antonov An-124
  X_AN225,  // Antonov An-225
  X_IL76,  // Ilyushin Il-76
  X_Q4,  // RQ-4 Global Hawk
  X_MQ9,  // MQ-9 Reaper / MQ-1
  X_KC135,  // KC-135 / Boeing 707 / E-6
  X_KC46,  // KC-46 Pegasus / Boeing 767-200
  X_R22,  // Robinson R22
  X_R44,  // Robinson R44
  X_R66,  // Robinson R66
  X_B06,  // Bell 206 JetRanger / LongRanger
  X_B505,  // Bell 505
  X_B47G,  // Bell 47 (bubble)
  X_UH1,  // Bell UH-1 Huey / 204/205/212
  X_AH1,  // Bell AH-1 Cobra
  X_B222,  // Bell 222/230
  X_B407,  // Bell 407
  X_B412,  // Bell 412
  X_B429,  // Bell 427/429
  X_B525,  // Bell 525
  X_H500,  // MD 500
  X_MD900,  // MD 900 Explorer (NOTAR)
  X_AS50,  // Airbus AS350 / H125
  X_AS55,  // Airbus AS355 TwinStar
  X_EC30,  // Airbus EC130 / H130
  X_EC20,  // Airbus EC120 Colibri
  X_EC35,  // Airbus EC135 / H135
  X_EC45,  // Airbus H145 / BK117
  X_AS65,  // Airbus Dauphin / H155
  X_H160,  // Airbus H160
  X_EC75,  // Airbus H175
  X_EC25,  // Airbus H225 / Super Puma
  X_GAZL,  // Aerospatiale Gazelle
  X_ALO3,  // Alouette II/III, Lama
  X_A109,  // Leonardo A109 / A119
  X_A139,  // Leonardo AW139
  X_A169,  // Leonardo AW169
  X_A189,  // Leonardo AW189 / AW149
  X_EH10,  // Leonardo AW101 / Merlin
  X_S76,  // Sikorsky S-76
  X_S92,  // Sikorsky S-92 / H-92
  X_H60,  // Sikorsky UH-60 Black Hawk / S-70
  X_H53,  // Sikorsky CH-53
  X_S61,  // Sikorsky S-61 / Sea King
  X_H47,  // Boeing CH-47 Chinook
  X_H46,  // Boeing CH-46 Sea Knight
  X_H64,  // Boeing AH-64 Apache
  X_TIGR,  // Airbus Tiger
  X_A129,  // Leonardo A129 Mangusta
  X_MI8,  // Mil Mi-8/17
  X_MI24,  // Mil Mi-24/35 Hind
  X_MI26,  // Mil Mi-26
  X_MI28,  // Mil Mi-28
  X_KA52,  // Kamov Ka-50/52
  X_KA32,  // Kamov Ka-27/32
  X_NH90,  // NHIndustries NH90
  X_LYNX,  // Westland Lynx / AW159
  X_EN28,  // Enstrom 280/480
  X_H269,  // Schweizer 300 / Hughes 269
  X_G2CA,  // Guimbal Cabri G2
  X_A320,  // Airbus A320 / A320neo
  X_B738,  // Boeing 737-800 / MAX 8 / P-8
  X_B789,  // Boeing 787-9
  X_CRJ9,  // Bombardier CRJ900/1000
  X_B763,  // Boeing 767-300
  X_A333,  // Airbus A330-300
  X_E195,  // Embraer 195
  X_E295,  // Embraer E195-E2
  X_E145,  // Embraer ERJ 145
  X_B744,  // Boeing 747-400 / classic
  X_A388,  // Airbus A380
  X_A343,  // Airbus A340-200/300
  X_MD11,  // McDonnell Douglas MD-11
  X_C17,  // Boeing C-17 Globemaster III
  X_C130,  // Lockheed C-130 Hercules, An-12, Il-18
  X_C27J,  // Alenia C-27J Spartan
  X_BE20,  // King Air 200/250/260
  X_B190,  // Beech 1900
  X_FA7X,  // Falcon 7X
  X_GLF5,  // Gulfstream II/III/V/G550
  X_G150,  // Gulfstream G100/G150/G200
  X_C650,  // Citation III/VI/VII
  X_EA50,  // Eclipse 500/550
  X_DH8A,  // Dash 8-100/200
  X_CL415,  // Canadair CL-215/415
  X_P06T,  // Tecnam P2006T
  X_SLING,  // Sling 2/4/TSi
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
