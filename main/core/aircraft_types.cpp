#include "core/aircraft_types.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace core::aircraft {

namespace {

struct TypeIcon {
  const char* type;
  Icon icon;
};

// ICAO type designators, sorted (binary search). Based on the lists on
// https://adsb-radar.com/help/icons.html, with a few corrected (the 737-800
// and the C-17 under one icon each, A350 and 787-10 with the twins, no
// regional jets on the MD-11) and more common types added.
const TypeIcon kTypes[] = {
    {"A10", Icon::Fighter},
    {"A109", Icon::Rotor},
    {"A119", Icon::Rotor},
    {"A124", Icon::Freighter},
    {"A139", Icon::Rotor},
    {"A169", Icon::Rotor},
    {"A189", Icon::Rotor},
    {"A19N", Icon::A320},
    {"A20N", Icon::A320},
    {"A21N", Icon::A320},
    {"A225", Icon::Freighter},
    {"A306", Icon::A330},
    {"A30B", Icon::A330},
    {"A310", Icon::A330},
    {"A318", Icon::A320},
    {"A319", Icon::A320},
    {"A320", Icon::A320},
    {"A321", Icon::A320},
    {"A332", Icon::A330},
    {"A333", Icon::A330},
    {"A337", Icon::A330},
    {"A338", Icon::A330},
    {"A339", Icon::A330},
    {"A342", Icon::A340},
    {"A343", Icon::A340},
    {"A345", Icon::A340},
    {"A346", Icon::A340},
    {"A359", Icon::A330},
    {"A35K", Icon::A330},
    {"A388", Icon::A380},
    {"A400", Icon::Freighter},
    {"AA1", Icon::SingleProp},
    {"AA5", Icon::SingleProp},
    {"AC90", Icon::TwinProp},
    {"AC95", Icon::TwinProp},
    {"AJET", Icon::Fighter},
    {"AN12", Icon::C130},
    {"AN22", Icon::Freighter},
    {"AN24", Icon::Turboprop},
    {"AN26", Icon::Turboprop},
    {"AN32", Icon::C130},
    {"AS21", Icon::Glider},
    {"AS32", Icon::Rotor},
    {"AS50", Icon::Rotor},
    {"AS55", Icon::Rotor},
    {"AS65", Icon::Rotor},
    {"ASK21", Icon::Glider},
    {"ASTR", Icon::BigBizJet},
    {"AT3T", Icon::SingleProp},
    {"AT43", Icon::Turboprop},
    {"AT44", Icon::Turboprop},
    {"AT45", Icon::Turboprop},
    {"AT46", Icon::Turboprop},
    {"AT5T", Icon::SingleProp},
    {"AT6T", Icon::SingleProp},
    {"AT72", Icon::Turboprop},
    {"AT73", Icon::Turboprop},
    {"AT75", Icon::Turboprop},
    {"AT76", Icon::Turboprop},
    {"AT8T", Icon::SingleProp},
    {"B06", Icon::Rotor},
    {"B06T", Icon::Rotor},
    {"B190", Icon::TwinProp},
    {"B350", Icon::TwinProp},
    {"B37M", Icon::B737},
    {"B38M", Icon::B737},
    {"B39M", Icon::B737},
    {"B3XM", Icon::B737},
    {"B407", Icon::Rotor},
    {"B412", Icon::Rotor},
    {"B429", Icon::Rotor},
    {"B461", Icon::RearJet},
    {"B462", Icon::RearJet},
    {"B463", Icon::RearJet},
    {"B505", Icon::Rotor},
    {"B609", Icon::Tiltrotor},
    {"B703", Icon::B767},
    {"B712", Icon::RearJet},
    {"B721", Icon::RearJet},
    {"B722", Icon::RearJet},
    {"B731", Icon::B737},
    {"B732", Icon::B737},
    {"B733", Icon::B737},
    {"B734", Icon::B737},
    {"B735", Icon::B737},
    {"B736", Icon::B737},
    {"B737", Icon::B737},
    {"B738", Icon::B737},
    {"B739", Icon::B737},
    {"B741", Icon::B747},
    {"B742", Icon::B747},
    {"B743", Icon::B747},
    {"B744", Icon::B747},
    {"B748", Icon::B747},
    {"B74D", Icon::B747},
    {"B74R", Icon::B747},
    {"B74S", Icon::B747},
    {"B752", Icon::B767},
    {"B753", Icon::B767},
    {"B762", Icon::B767},
    {"B763", Icon::B767},
    {"B764", Icon::B767},
    {"B772", Icon::B777},
    {"B773", Icon::B777},
    {"B778", Icon::B777},
    {"B779", Icon::B777},
    {"B77L", Icon::B777},
    {"B77W", Icon::B777},
    {"B788", Icon::B787},
    {"B789", Icon::B787},
    {"B78X", Icon::B787},
    {"BALL", Icon::Airship},
    {"BCS1", Icon::A320},
    {"BCS3", Icon::A320},
    {"BE10", Icon::TwinProp},
    {"BE20", Icon::TwinProp},
    {"BE23", Icon::SingleProp},
    {"BE24", Icon::SingleProp},
    {"BE30", Icon::TwinProp},
    {"BE33", Icon::SingleProp},
    {"BE35", Icon::SingleProp},
    {"BE36", Icon::SingleProp},
    {"BE40", Icon::BizJet},
    {"BE55", Icon::TwinProp},
    {"BE58", Icon::TwinProp},
    {"BE60", Icon::TwinProp},
    {"BE76", Icon::TwinProp},
    {"BE95", Icon::TwinProp},
    {"BE9L", Icon::TwinProp},
    {"BE9T", Icon::TwinProp},
    {"BK17", Icon::Rotor},
    {"BLCF", Icon::B747},
    {"BN2P", Icon::TwinProp},
    {"C130", Icon::C130},
    {"C135", Icon::B767},
    {"C150", Icon::SingleProp},
    {"C152", Icon::SingleProp},
    {"C160", Icon::C130},
    {"C162", Icon::SingleProp},
    {"C17", Icon::Freighter},
    {"C170", Icon::SingleProp},
    {"C172", Icon::SingleProp},
    {"C175", Icon::SingleProp},
    {"C177", Icon::SingleProp},
    {"C180", Icon::SingleProp},
    {"C182", Icon::SingleProp},
    {"C185", Icon::SingleProp},
    {"C195", Icon::SingleProp},
    {"C205", Icon::SingleProp},
    {"C206", Icon::SingleProp},
    {"C207", Icon::SingleProp},
    {"C208", Icon::SingleProp},
    {"C210", Icon::SingleProp},
    {"C25A", Icon::BizJet},
    {"C25B", Icon::BizJet},
    {"C25C", Icon::BizJet},
    {"C25M", Icon::BizJet},
    {"C27J", Icon::C130},
    {"C295", Icon::Turboprop},
    {"C303", Icon::TwinProp},
    {"C30J", Icon::C130},
    {"C310", Icon::TwinProp},
    {"C340", Icon::TwinProp},
    {"C402", Icon::TwinProp},
    {"C404", Icon::TwinProp},
    {"C414", Icon::TwinProp},
    {"C421", Icon::TwinProp},
    {"C425", Icon::TwinProp},
    {"C441", Icon::TwinProp},
    {"C5", Icon::Freighter},
    {"C500", Icon::BizJet},
    {"C501", Icon::BizJet},
    {"C510", Icon::BizJet},
    {"C525", Icon::BizJet},
    {"C526", Icon::BizJet},
    {"C550", Icon::BizJet},
    {"C551", Icon::BizJet},
    {"C55B", Icon::BizJet},
    {"C560", Icon::BizJet},
    {"C56X", Icon::BizJet},
    {"C5M", Icon::Freighter},
    {"C650", Icon::BizJet},
    {"C680", Icon::BizJet},
    {"C68A", Icon::BizJet},
    {"C700", Icon::BizJet},
    {"C72R", Icon::SingleProp},
    {"C750", Icon::BizJet},
    {"C77R", Icon::SingleProp},
    {"C82R", Icon::SingleProp},
    {"C919", Icon::A320},
    {"CL2P", Icon::Amphibian},
    {"CL2T", Icon::Amphibian},
    {"CL30", Icon::BigBizJet},
    {"CL35", Icon::BigBizJet},
    {"CL4T", Icon::Amphibian},
    {"CL60", Icon::BigBizJet},
    {"CN35", Icon::Turboprop},
    {"COL3", Icon::SingleProp},
    {"COL4", Icon::SingleProp},
    {"CRJ1", Icon::CRJ},
    {"CRJ2", Icon::CRJ},
    {"CRJ7", Icon::CRJ},
    {"CRJ9", Icon::CRJ},
    {"CRJX", Icon::CRJ},
    {"CRUZ", Icon::SingleProp},
    {"D328", Icon::Turboprop},
    {"DA20", Icon::SingleProp},
    {"DA40", Icon::SingleProp},
    {"DA42", Icon::TwinProp},
    {"DA62", Icon::TwinProp},
    {"DC10", Icon::MD11},
    {"DC91", Icon::RearJet},
    {"DC93", Icon::RearJet},
    {"DC95", Icon::RearJet},
    {"DG1T", Icon::Glider},
    {"DH8A", Icon::Turboprop},
    {"DH8B", Icon::Turboprop},
    {"DH8C", Icon::Turboprop},
    {"DH8D", Icon::Turboprop},
    {"DHC2", Icon::Amphibian},
    {"DHC3", Icon::Amphibian},
    {"DHC6", Icon::TwinProp},
    {"DHC7", Icon::Turboprop},
    {"DISC", Icon::Glider},
    {"DV20", Icon::SingleProp},
    {"E120", Icon::TwinProp},
    {"E135", Icon::ERJ},
    {"E145", Icon::ERJ},
    {"E170", Icon::E195},
    {"E175", Icon::E195},
    {"E190", Icon::E195},
    {"E195", Icon::E195},
    {"E290", Icon::E195},
    {"E295", Icon::E195},
    {"E35L", Icon::ERJ},
    {"E3CF", Icon::B767},
    {"E3TF", Icon::B767},
    {"E45X", Icon::ERJ},
    {"E50P", Icon::BizJet},
    {"E545", Icon::BizJet},
    {"E550", Icon::BizJet},
    {"E55P", Icon::BizJet},
    {"E6", Icon::B767},
    {"E75L", Icon::E195},
    {"E75S", Icon::E195},
    {"E767", Icon::B767},
    {"EC20", Icon::Rotor},
    {"EC25", Icon::Rotor},
    {"EC30", Icon::Rotor},
    {"EC35", Icon::Rotor},
    {"EC45", Icon::Rotor},
    {"EC55", Icon::Rotor},
    {"EC75", Icon::Rotor},
    {"EN28", Icon::Rotor},
    {"EN48", Icon::Rotor},
    {"EUFI", Icon::Fighter},
    {"F100", Icon::RearJet},
    {"F14", Icon::Fighter},
    {"F15", Icon::Fighter},
    {"F16", Icon::Fighter},
    {"F18", Icon::Fighter},
    {"F18S", Icon::Fighter},
    {"F22", Icon::Fighter},
    {"F27", Icon::Turboprop},
    {"F2TH", Icon::Falcon},
    {"F35", Icon::Fighter},
    {"F4", Icon::Fighter},
    {"F5", Icon::Fighter},
    {"F50", Icon::Turboprop},
    {"F70", Icon::RearJet},
    {"F900", Icon::Falcon},
    {"FA10", Icon::Falcon},
    {"FA20", Icon::Falcon},
    {"FA50", Icon::Falcon},
    {"FA6X", Icon::Falcon},
    {"FA7X", Icon::Falcon},
    {"FA8X", Icon::Falcon},
    {"G280", Icon::BigBizJet},
    {"GA5C", Icon::BigBizJet},
    {"GA6C", Icon::BigBizJet},
    {"GA7C", Icon::BigBizJet},
    {"GA8", Icon::SingleProp},
    {"GA8C", Icon::BigBizJet},
    {"GALX", Icon::BigBizJet},
    {"GAZL", Icon::Rotor},
    {"GL5T", Icon::BigBizJet},
    {"GL7T", Icon::BigBizJet},
    {"GLEX", Icon::BigBizJet},
    {"GLF2", Icon::BigBizJet},
    {"GLF3", Icon::BigBizJet},
    {"GLF4", Icon::BigBizJet},
    {"GLF5", Icon::BigBizJet},
    {"GLF6", Icon::BigBizJet},
    {"GLID", Icon::Glider},
    {"GRIF", Icon::Fighter},
    {"H160", Icon::Rotor},
    {"H25A", Icon::BizJet},
    {"H25B", Icon::BizJet},
    {"H25C", Icon::BizJet},
    {"H47", Icon::Rotor},
    {"H500", Icon::Rotor},
    {"H60", Icon::Rotor},
    {"H64", Icon::Rotor},
    {"HAWK", Icon::Fighter},
    {"HDJT", Icon::BizJet},
    {"IL76", Icon::Freighter},
    {"JS41", Icon::Turboprop},
    {"K35E", Icon::B767},
    {"K35R", Icon::B767},
    {"K8", Icon::Fighter},
    {"KC46", Icon::B767},
    {"KODI", Icon::SingleProp},
    {"L101", Icon::MD11},
    {"L39", Icon::Fighter},
    {"L410", Icon::TwinProp},
    {"LJ31", Icon::BizJet},
    {"LJ35", Icon::BizJet},
    {"LJ40", Icon::BizJet},
    {"LJ45", Icon::BizJet},
    {"LJ55", Icon::BizJet},
    {"LJ60", Icon::BizJet},
    {"LJ70", Icon::BizJet},
    {"LJ75", Icon::BizJet},
    {"LS8", Icon::Glider},
    {"M20J", Icon::SingleProp},
    {"M20K", Icon::SingleProp},
    {"M20P", Icon::SingleProp},
    {"M20T", Icon::SingleProp},
    {"M346", Icon::Fighter},
    {"MD11", Icon::MD11},
    {"MD52", Icon::Rotor},
    {"MD60", Icon::Rotor},
    {"MD81", Icon::RearJet},
    {"MD82", Icon::RearJet},
    {"MD83", Icon::RearJet},
    {"MD87", Icon::RearJet},
    {"MD88", Icon::RearJet},
    {"MD90", Icon::RearJet},
    {"MG29", Icon::Fighter},
    {"MI17", Icon::Rotor},
    {"MI24", Icon::Rotor},
    {"MI8", Icon::Rotor},
    {"MIR2", Icon::Fighter},
    {"MRTT", Icon::A330},
    {"MU2", Icon::TwinProp},
    {"NH90", Icon::Rotor},
    {"P180", Icon::TwinProp},
    {"P210", Icon::SingleProp},
    {"P28A", Icon::SingleProp},
    {"P28B", Icon::SingleProp},
    {"P28R", Icon::SingleProp},
    {"P28T", Icon::SingleProp},
    {"P28U", Icon::SingleProp},
    {"P3", Icon::C130},
    {"P32R", Icon::SingleProp},
    {"P32T", Icon::SingleProp},
    {"P46T", Icon::SingleProp},
    {"P68", Icon::TwinProp},
    {"PA11", Icon::SingleProp},
    {"PA12", Icon::SingleProp},
    {"PA18", Icon::SingleProp},
    {"PA20", Icon::SingleProp},
    {"PA22", Icon::SingleProp},
    {"PA23", Icon::TwinProp},
    {"PA24", Icon::SingleProp},
    {"PA27", Icon::TwinProp},
    {"PA28", Icon::SingleProp},
    {"PA30", Icon::TwinProp},
    {"PA31", Icon::TwinProp},
    {"PA32", Icon::SingleProp},
    {"PA34", Icon::TwinProp},
    {"PA38", Icon::SingleProp},
    {"PA44", Icon::TwinProp},
    {"PA46", Icon::SingleProp},
    {"PAY1", Icon::TwinProp},
    {"PAY2", Icon::TwinProp},
    {"PAY3", Icon::TwinProp},
    {"PAY4", Icon::TwinProp},
    {"PBY", Icon::Amphibian},
    {"PC12", Icon::SingleProp},
    {"PC21", Icon::SingleProp},
    {"PC24", Icon::BizJet},
    {"PC6T", Icon::SingleProp},
    {"PRM1", Icon::BizJet},
    {"R22", Icon::Rotor},
    {"R44", Icon::Rotor},
    {"R66", Icon::Rotor},
    {"RFAL", Icon::Fighter},
    {"RJ1H", Icon::RearJet},
    {"RJ70", Icon::RearJet},
    {"RJ85", Icon::RearJet},
    {"RV10", Icon::SingleProp},
    {"RV12", Icon::SingleProp},
    {"RV14", Icon::SingleProp},
    {"RV4", Icon::SingleProp},
    {"RV6", Icon::SingleProp},
    {"RV7", Icon::SingleProp},
    {"RV8", Icon::SingleProp},
    {"RV9", Icon::SingleProp},
    {"S22T", Icon::SingleProp},
    {"S61", Icon::Rotor},
    {"S76", Icon::Rotor},
    {"S92", Icon::Rotor},
    {"SB20", Icon::Turboprop},
    {"SF34", Icon::Turboprop},
    {"SF50", Icon::BizJet},
    {"SH36", Icon::Turboprop},
    {"SHIP", Icon::Airship},
    {"SR20", Icon::SingleProp},
    {"SR22", Icon::SingleProp},
    {"SU27", Icon::Fighter},
    {"SU30", Icon::Fighter},
    {"SU35", Icon::Fighter},
    {"SW4", Icon::TwinProp},
    {"T210", Icon::SingleProp},
    {"T34", Icon::SingleProp},
    {"T38", Icon::Fighter},
    {"T45", Icon::Fighter},
    {"T50", Icon::Fighter},
    {"T6", Icon::SingleProp},
    {"T7", Icon::Fighter},
    {"TBM7", Icon::SingleProp},
    {"TBM8", Icon::SingleProp},
    {"TBM9", Icon::SingleProp},
    {"TOR", Icon::Fighter},
    {"UH1", Icon::Rotor},
    {"ULAC", Icon::Ultralight},
    {"V22", Icon::Tiltrotor},
};

}  // namespace

const char* iconName(Icon icon) {
  switch (icon) {
    case Icon::Generic: return "a0";
    case Icon::Light: return "a1";
    case Icon::Small: return "a2";
    case Icon::Large: return "a3";
    case Icon::HighVortex: return "a4";
    case Icon::Heavy: return "a5";
    case Icon::HighPerf: return "a6";
    case Icon::Rotor: return "a7";
    case Icon::Drone: return "b0";
    case Icon::Glider: return "b1";
    case Icon::Airship: return "b2";
    case Icon::Parachute: return "b3";
    case Icon::Ultralight: return "b4";
    case Icon::Ground: return "c0";
    case Icon::A320: return "a320";
    case Icon::A330: return "a330";
    case Icon::A340: return "a340";
    case Icon::A380: return "a380";
    case Icon::B737: return "b737";
    case Icon::B747: return "b747";
    case Icon::B767: return "b767";
    case Icon::B777: return "b777";
    case Icon::B787: return "b787";
    case Icon::MD11: return "md11";
    case Icon::RearJet: return "f100";
    case Icon::CRJ: return "crjx";
    case Icon::ERJ: return "erj";
    case Icon::E195: return "e195";
    case Icon::Turboprop: return "dh8a";
    case Icon::TwinProp: return "beechcraft";
    case Icon::SingleProp: return "cessna";
    case Icon::BizJet: return "learjet";
    case Icon::BigBizJet: return "glf5";
    case Icon::Falcon: return "fa7x";
    case Icon::C130: return "c130";
    case Icon::Freighter: return "heavyfreighter";
    case Icon::Fighter: return "fighter";
    case Icon::Tiltrotor: return "tiltrotorcraft";
    case Icon::Amphibian: return "amphibian";
    case Icon::kCount: break;
  }
  return "a0";
}

int baseSize(Icon icon) {
  switch (icon) {
    case Icon::Ground: return 18;
    case Icon::Parachute: return 20;
    case Icon::Drone: return 22;
    case Icon::Ultralight: return 28;
    case Icon::Airship:
    case Icon::Light:
    case Icon::SingleProp: return 30;
    case Icon::Glider: return 32;
    case Icon::Small:
    case Icon::TwinProp:
    case Icon::BizJet: return 34;
    case Icon::HighPerf:
    case Icon::Fighter:
    case Icon::Amphibian: return 36;
    case Icon::Rotor:  // thin rotor blades: a little bigger so they still show
    case Icon::BigBizJet:
    case Icon::Falcon:
    case Icon::CRJ:
    case Icon::ERJ: return 38;
    case Icon::Generic:
    case Icon::Tiltrotor:
    case Icon::E195:
    case Icon::Turboprop:
    case Icon::RearJet: return 40;
    case Icon::Large:
    case Icon::A320:
    case Icon::B737: return 42;
    case Icon::C130: return 44;
    case Icon::HighVortex:
    case Icon::B767: return 46;
    case Icon::Heavy:
    case Icon::A330:
    case Icon::B787:
    case Icon::MD11: return 48;
    case Icon::A340:
    case Icon::B777: return 50;
    case Icon::B747:
    case Icon::Freighter: return 54;
    case Icon::A380: return 56;
    case Icon::kCount: break;
  }
  return 40;
}

bool iconForType(const char* type, Icon* out) {
  if (type == nullptr || type[0] == '\0') return false;
  char key[8];
  size_t n = 0;
  for (; type[n] != '\0' && n < sizeof(key) - 1; ++n) {
    key[n] = static_cast<char>(std::toupper(static_cast<unsigned char>(type[n])));
  }
  if (type[n] != '\0') return false;  // longer than any designator
  key[n] = '\0';
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
  if (category != nullptr && category[0] != '\0') {
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
