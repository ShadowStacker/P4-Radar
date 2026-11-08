#!/usr/bin/env python3
"""Writes main/core/aircraft_types.{h,cpp}: the icon list and the ICAO type table.

ICONS below are the size-class icons, the ADS-B Radar icons
(https://adsb-radar.com) and the family icons of aircraft_families.py; every
entry of aircraft_catalog.py adds one more icon for its own types (a type in
the catalogue takes its own icon over a family one).

  python3 scripts/gen_aircraft_types.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from aircraft_catalog import CATALOG  # noqa: E402

os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

# (enum name, mask name, base size px at 800, comment, types)
ICONS = [
 # emitter categories
 ('Generic','a0',40,'A0 / unknown',''),
 ('Light','g_single_high',30,'A1 light (unknown type): a high-wing single',''),
 ('Small','a2',34,'A2 small',''),
 ('Large','a3',42,'A3 large',''),
 ('HighVortex','a4',46,'A4 (757 class)',''),
 ('Heavy','a5',48,'A5 heavy',''),
 ('HighPerf','a6',36,'A6 high performance',''),
 ('Rotor','a7',38,'A7 rotorcraft (unknown type)','UHEL'),
 ('Drone','b0',22,'B6 unmanned',''),
 ('Glider','x_glider',32,'B1 glider (a sailplane)',''),
 ('Airship','x_balloon',30,'B2 lighter than air (a hot air balloon)',''),
 ('Parachute','b3',20,'B3 parachutist',''),
 ('Ultralight','b4',28,'B4 ultralight','ULAC PIAT'),
 ('Ground','c0',18,'C1-C7 surface vehicles; ground stations','TWR GND SERV EMER'),
 # airliners (ADS-B Radar icons)
 ('A320','a320',42,'A320 family, A220, C919','A318 A319 A320 A321 A19N A20N A21N BCS1 BCS3 C919'),
 ('A330','a330',48,'A300/310/330, A350','A306 A30B A310 A332 A333 A337 A338 A339 A359 A35K A3ST'),
 ('A340','a340',50,'A340','A342 A343 A345 A346'),
 ('A380','a380',56,'A380','A388'),
 ('B737','b737',42,'737 family, P-8','B731 B732 B733 B734 B735 B736 B737 B738 B739 B37M B38M B39M B3XM P8 E737'),
 ('B747','b747',54,'747','B741 B742 B743 B744 B748 B74S B74R B74D BLCF'),
 ('B767','b767',46,'757, 767, KC-46','B752 B753 B762 B763 B764 K46 KC46'),
 ('B777','b777',50,'777','B772 B773 B77L B77W B778 B779'),
 ('B787','b787',48,'787','B788 B789 B78X'),
 ('MD11','md11',48,'MD-11, DC-10, KC-10, L-1011','MD11 DC10 L101'),
 ('RearJet','f100',40,'rear-engine jets: MD-80/90, F100, 717, 727, DC-9, BAe 146','F100 F70 MD81 MD82 MD83 MD87 MD88 MD90 B712 B721 B722 DC91 DC92 DC93 DC94 DC95 B461 B462 B463 RJ70 RJ85 RJ1H T154 YK42 IL62'),
 ('CRJ','crjx',38,'CRJ','CRJ1 CRJ2 CRJ7 CRJ9 CRJX'),
 ('ERJ','erj',38,'ERJ, Legacy 600, Do 328JET','E135 E145 E35L E45X J328'),
 ('E195','e195',40,'E-Jets, Superjet, ARJ21','E170 E175 E75L E75S E190 E195 E290 E295 SU95 AJ27'),
 ('Turboprop','dh8a',40,'regional turboprops: Dash 8, ATR, Saab, Do 328','DH8A DH8B DH8C DH8D AT43 AT44 AT45 AT46 AT72 AT73 AT75 AT76 F27 F50 SB20 SF34 D328 C295 CN35 AN24 AN26 AN32 DHC7 SH33 SH36 MA60 IL14'),
 ('TwinProp','beechcraft',36,'King Air','BE20 BE30 BE9L BE9T BE10 B350 B190'),
 ('BizJet','learjet',34,'Citation, Learjet, Phenom, Hawker','C25A C25B C25C C25M C500 C501 C510 C525 C526 C550 C551 C55B C560 C56X C650 C680 C68A C700 C750 LJ31 LJ35 LJ40 LJ45 LJ55 LJ60 LJ70 LJ75 E50P E55P PRM1 H25A H25B H25C HDJT SF50 BE40 PC24 E545 E550'),
 ('BigBizJet','glf5',38,'Gulfstream, Global, Challenger','GLF2 GLF3 GLF4 GLF5 GLF6 GA5C GA6C GA7C GA8C G280 GLEX GL5T GL7T GL8T CL30 CL35 CL60 ASTR GALX'),
 ('Falcon','fa7x',38,'Dassault Falcon','FA7X FA8X FA10 FA20 FA50 F900 F2TH FA6X'),
 ('C130','c130',44,'four-turboprop transports: C-130, A400M, P-3','C130 C30J C27J C160 AN12 P3 L188 A400 IL18'),
 ('Freighter','heavyfreighter',54,'heavy jet freighters: C-17, C-5, An-124, Il-76','A124 A225 IL76 C5 C5M C17 AN22'),
 ('Tiltrotor','tiltrotorcraft',40,'V-22, AW609','V22 B609'),
 ('Amphibian','amphibian',36,'water bombers and floatplanes','CL2T CL4T CL2P PBY'),
 # drawn in the same style (scripts/aircraft_families.py)
 ('SingleHigh','g_single_high',30,'Cessna 150-210 and other high-wing singles','C150 C152 C162 C170 C172 C175 C177 C180 C182 C185 C195 C205 C206 C207 C210 P210 T210 C72R C77R C82R C82S C82T PA18 PA20 PA22 PA12 PA14 J3 CH7A BL8 M5 M6 M7 MT7 GA8 C42 SAVG'),
 ('SingleLow','g_single_low',30,'Piper PA-28/32, Cirrus, Diamond, Bonanza, Mooney, RV','PA24 PA28 P28A P28B P28R P28T P28U PA32 P32R P32T PA38 M20P M20T M20J M20K M20R SR20 SR22 S22T DA40 DV20 DA20 BE33 BE35 BE36 BE23 BE24 BE19 AA1 AA5 RV4 RV6 RV7 RV8 RV9 RV10 RV12 RV14 TB9 TB10 TB20 TB21 TOBA COL3 COL4 LANC G115 F260 DR40 CRUZ'),
 ('TurbopropSingle','g_turboprop_single',34,'PC-12, TBM, PA-46, T-6, Super Tucano, Air Tractor','PC12 PC21 PC9 PC7 TUCA HURK TBM7 TBM8 TBM9 PA46 P46T T6 TEX2 E314 T34 AT3T AT4T AT5T AT6T AT8T'),
 ('TurbopropSingleHigh','g_turboprop_single_high',34,'Caravan, Kodiak, Beaver, Otter, Porter','C208 KODI PC6T PC6P DHC2 DHC3 DH3T'),
 ('TwinPiston','g_twin_piston',32,'Baron, Seneca, Seminole, Cessna 310-421, DA42','BE55 BE58 BE60 BE76 BE95 PA23 PA27 PA30 PA31 PA34 PA44 C303 C310 C320 C335 C340 C401 C402 C411 C414 C421 DA42 DA62 AEST'),
 ('TwinTurboprop','g_twin_turboprop',36,'Beech 99, Metro, Jetstream, Conquest, MU-2, Cheyenne','BE99 C425 C441 MU2 PAY1 PAY2 PAY3 PAY4 SW2 SW3 SW4 JS31 JS32 JS41 E120 E121 PA42 AC90 AC95 AC11 P180 F406'),
 ('TwinHigh','g_twin_high',38,'Twin Otter, Islander, Do 228, Let 410, CASA 212, Skyvan, P68','DHC6 BN2P BN2T D228 L410 C212 AN28 M28 DO28 SC7 P68'),
 ('B707','g_b707',48,'707, KC-135, E-6','B701 B703 B720 K35R K35E K35T C135 E6 R135'),
 ('Awacs','g_awacs',50,'AWACS: E-3, A-50, E-767','E3TF E3CF A50 E767'),
 ('Hawkeye','g_hawkeye',40,'E-2 Hawkeye, C-2','E2 C2'),
 ('B52','g_b52',56,'B-52','B52'),
 ('B1','g_b1',50,'B-1, Tu-160','B1 T160'),
 ('B2','g_b2',54,'B-2','B2'),
 ('Reaper','g_mq9',32,'MQ-9, MQ-1, Global Hawk, Heron','MQ9 Q9 MQ1 Q1 Q4 RQ4 HRON'),
 ('FighterSingle','g_fighter_single',34,'single-fin fighters: F-16, F-4, F-5, Tornado, A-4','F16 F2 JF17 F4 F5 TOR A4 SU25 MG21 AMX'),
 ('FighterTwin','g_fighter_twin',38,'twin-fin fighters: F-15, F/A-18, F-22, F-35, F-14, Su-27 family, MiG-29','F15 F18 F18S EA18 F22 F35 F14 SU27 SU30 SU33 SU34 SU35 SU57 MG29 MG31 J11 J15 J20'),
 ('FighterDelta','g_fighter_delta',36,'delta fighters: Eurofighter, Rafale, Gripen, Mirage 2000, J-10','EUFI RFAL GRIF MIR2 J10 KFIR'),
 ('Attack','g_attack',38,'A-10','A10'),
 ('JetTrainer','g_jet_trainer',32,'jet trainers: T-38, Hawk, L-39, M-346, Alpha Jet, T-45, T-7','T38 T45 HAWK L39 L159 M346 AJET T7 YK13 K8 T50'),
 ('Heli2','g_heli_2',32,'two-blade helicopters: Robinson, Bell 206/505/212/222, UH-1','R22 R44 R66 B06 B06T B47G B505 B212 B222 UH1'),
 ('Heli3','g_heli_3',34,'three-blade helicopters: AS350/H125, AS355, EC130, Gazelle','AS50 AS55 EC30 EC20 GAZL LAMA ALO2 ALO3 EN28 EN48 H269'),
 ('Heli4','g_heli_4',36,'four-blade helicopters: H135, H145, Bell 407/429/412, A109, S-76, Dauphin','EC35 EC45 B407 B427 B429 B430 B412 A109 A119 A169 S76 AS65 BK17 MD60'),
 ('Heli5','g_heli_5',40,'five-blade helicopters: AW139, H160, H175, H225, Mi-8/17, S-61, AW101','A139 A149 A189 H160 EC55 EC75 EC25 MI8 MI14 MI17 S61 EH10 H500'),
 ('HeliLarge','g_heli_large',42,'big four-blade helicopters: UH-60, S-92, NH90, Super Puma, CH-53','H60 S70 S92 NH90 AS32 AS3B H53 H53S MI26'),
 ('HeliTandem','g_heli_tandem',44,'tandem rotors: CH-47, CH-46','H47 H46'),
 ('HeliAttack','g_heli_attack',38,'attack helicopters: AH-64, AH-1, Mi-24/28, Tiger, Z-10','H64 H1 MI24 MI28 TIGR A129 WZ10'),
 ('HeliCoax','g_heli_coax',38,'coaxial rotors: Kamov','KA26 KA27 KA32 KA50 KA52'),
]


def catalog_size(fn):
    """Icon size from the aircraft's real size: 30 px at 11 m (a Cessna 172), to the 0.3 power."""
    s = fn()
    d = max(s.span, s.length)
    return max(22, min(58, round(30 * (d / 11.0) ** 0.3)))


for key, desc, types, fn in CATALOG:
    ICONS.append(('X_' + key.upper(), 'x_' + key, catalog_size(fn), desc, types))

seen = {}
for e, m, sz, c, types in ICONS:
    for t in types.split():
        seen[t] = e  # later (more specific) entries win

# Every other ICAO type designator gets the icon of its class (scripts/data/
# icao_type_classes.txt, from ICAO Doc 8643): engines, their kind and the wake
# category say at least "light twin", "turboprop single", "bizjet",
# "two-turbine helicopter"...
def class_icon(cls, wtc):
    kind, n, eng = cls[0], cls[1], cls[2]
    n = int(n) if n.isdigit() else 0
    if kind in 'LS':
        if eng in 'PE':
            return {1: 'Light', 2: 'TwinPiston', 3: 'X_TRIMOTOR'}.get(n, 'X_DC6')
        if eng == 'T':
            return {1: 'TurbopropSingle', 2: 'TwinTurboprop' if wtc == 'L' else 'Turboprop'}.get(n, 'C130')
        if eng == 'J':
            if n == 1:
                return 'JetTrainer'
            if n == 2:
                return {'L': 'BizJet', 'M': 'BigBizJet'}.get(wtc, 'Heavy')
            if n == 3:
                return 'Falcon' if wtc != 'H' else 'MD11'
            return 'X_BAE146' if wtc == 'M' else 'Freighter'
    if kind == 'H':
        return 'Heli2' if n == 1 and eng == 'P' else {1: 'Heli3', 2: 'Heli4'}.get(n, 'Heli5')
    if kind == 'A':
        return {1: 'X_LA4', 2: 'X_G21'}.get(n, 'Amphibian')
    return {'G': 'X_GYRO', 'R': 'Tiltrotor', 'D': 'X_QUAD', 'B': 'Airship', 'V': 'Ground'}.get(kind)


names = {ic[0] for ic in ICONS}
by_class = 0
from_class = set()
for line in open(os.path.join('scripts', 'data', 'icao_type_classes.txt')):
    if line.startswith('#') or not line.strip():
        continue
    code, cls, wtc = line.split()
    if code in seen:
        continue
    e = class_icon(cls, wtc)
    if e in names:
        seen[code] = e
        from_class.add(code)
        by_class += 1
print(by_class, 'types by class')

# Drop icons left with no types (all of theirs went to more specific ones),
# except the size-class icons (Generic ... Ground), used without a type.
CLASS_ICONS = 14
used = set(seen.values())
ICONS = [ic for i, ic in enumerate(ICONS) if i < CLASS_ICONS or ic[0] in used]
print(len(seen), 'types,', len(ICONS), 'icons')
hdr = '''#pragma once

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
'''
for e, m, sz, c, t in ICONS:
    hdr += f'  {e},  // {c}\n'
hdr += '''  kCount
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
'''
open('main/core/aircraft_types.h','w').write(hdr)
cpp = '''#include "core/aircraft_types.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace core::aircraft {

namespace {

struct TypeIcon {
  const char* type;
  Icon icon;
};

// ICAO type designators, sorted (binary search). The airliner groups follow
// https://adsb-radar.com/help/icons.html, corrected in places (the 737-800
// and the C-17 under one icon each, A350 and 787-10 with the twins, no
// regional jets on the MD-11); the rest match the generated icons: each
// type goes with the shape it really has (props or jets, how many engines
// and where, rotor blades).
const TypeIcon kTypes[] = {
'''
for t, e in sorted(seen.items()):
    cpp += f'    {{"{t}", Icon::{e}}},' + ('  // by class\n' if t in from_class else '\n')
cpp += '''};

struct IconInfo {
  const char* mask;
  uint8_t size;
};

const IconInfo kInfo[] = {
'''
for e, m, sz, c, t in ICONS:
    cpp += f'    {{"{m}", {sz}}},  // {e}\n'
cpp += '''};
static_assert(sizeof(kInfo) / sizeof(kInfo[0]) == static_cast<size_t>(Icon::kCount), "one entry per icon");

}  // namespace

const char* iconName(Icon icon) {
  const auto i = static_cast<size_t>(icon);
  return i < static_cast<size_t>(Icon::kCount) ? kInfo[i].mask : "a0";
}

int baseSize(Icon icon) {
  const auto i = static_cast<size_t>(icon);
  return i < static_cast<size_t>(Icon::kCount) ? kInfo[i].size : 40;
}

bool iconForType(const char* type, Icon* out) {
  if (type == nullptr || type[0] == '\\0') return false;
  char key[8];
  size_t n = 0;
  for (; type[n] != '\\0' && n < sizeof(key) - 1; ++n) {
    key[n] = static_cast<char>(std::toupper(static_cast<unsigned char>(type[n])));
  }
  if (type[n] != '\\0') return false;  // longer than any designator
  key[n] = '\\0';
  const TypeIcon* end = kTypes + sizeof(kTypes) / sizeof(kTypes[0]);
  const TypeIcon* it =
      std::lower_bound(kTypes, end, key, [](const TypeIcon& t, const char* k) { return std::strcmp(t.type, k) < 0; });
  if (it == end || std::strcmp(it->type, key) != 0) return false;
  *out = it->icon;
  return true;
}

Icon iconFor(const char* type, const char* category) {
  Icon icon;
  if (iconForType(type, &icon)) return icon;
  if (category != nullptr && category[0] != '\\0') {
    const char c = category[1];
    switch (category[0]) {
      case 'A':
        switch (c) {
          case '1': return Icon::Light;
          case '2': return Icon::Small;
          case '3': return Icon::Large;
          case '4': return Icon::HighVortex;
          case '5': return Icon::Heavy;
          case '6': return Icon::HighPerf;
          case '7': return Icon::Rotor;
          default: break;
        }
        break;
      case 'B':
        switch (c) {
          case '1': return Icon::Glider;
          case '2': return Icon::Airship;
          case '3': return Icon::Parachute;
          case '4': return Icon::Ultralight;
          case '6': return Icon::Drone;
          default: break;
        }
        break;
      case 'C':
        if (c >= '1' && c <= '7') return Icon::Ground;
        break;
      default: break;
    }
  }
  return Icon::Generic;
}

}  // namespace core::aircraft
'''
open('main/core/aircraft_types.cpp','w').write(cpp)
