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

// ICAO type designators, sorted (binary search). The airliner groups follow
// https://adsb-radar.com/help/icons.html, corrected in places (the 737-800
// and the C-17 under one icon each, A350 and 787-10 with the twins, no
// regional jets on the MD-11); the rest match the generated icons: each
// type goes with the shape it really has (props or jets, how many engines
// and where, rotor blades).
const TypeIcon kTypes[] = {
    {"A10", Icon::Attack},
    {"A109", Icon::Heli4},
    {"A119", Icon::Heli4},
    {"A124", Icon::Freighter},
    {"A129", Icon::HeliAttack},
    {"A139", Icon::Heli5},
    {"A149", Icon::Heli5},
    {"A169", Icon::Heli4},
    {"A189", Icon::Heli5},
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
    {"A3ST", Icon::A330},
    {"A4", Icon::FighterSingle},
    {"A400", Icon::C130},
    {"A50", Icon::Awacs},
    {"AA1", Icon::SingleLow},
    {"AA5", Icon::SingleLow},
    {"AC11", Icon::TwinTurboprop},
    {"AC90", Icon::TwinTurboprop},
    {"AC95", Icon::TwinTurboprop},
    {"AEST", Icon::TwinPiston},
    {"AJ27", Icon::E195},
    {"AJET", Icon::JetTrainer},
    {"ALO2", Icon::Heli3},
    {"ALO3", Icon::Heli3},
    {"AMX", Icon::FighterSingle},
    {"AN12", Icon::C130},
    {"AN22", Icon::Freighter},
    {"AN24", Icon::Turboprop},
    {"AN26", Icon::Turboprop},
    {"AN28", Icon::TwinHigh},
    {"AN32", Icon::Turboprop},
    {"AS21", Icon::Glider},
    {"AS32", Icon::HeliLarge},
    {"AS3B", Icon::HeliLarge},
    {"AS50", Icon::Heli3},
    {"AS55", Icon::Heli3},
    {"AS65", Icon::Heli4},
    {"ASK21", Icon::Glider},
    {"ASK3", Icon::Glider},
    {"ASTR", Icon::BigBizJet},
    {"ASW2", Icon::Glider},
    {"AT3T", Icon::TurbopropSingle},
    {"AT43", Icon::Turboprop},
    {"AT44", Icon::Turboprop},
    {"AT45", Icon::Turboprop},
    {"AT46", Icon::Turboprop},
    {"AT4T", Icon::TurbopropSingle},
    {"AT5T", Icon::TurbopropSingle},
    {"AT6T", Icon::TurbopropSingle},
    {"AT72", Icon::Turboprop},
    {"AT73", Icon::Turboprop},
    {"AT75", Icon::Turboprop},
    {"AT76", Icon::Turboprop},
    {"AT8T", Icon::TurbopropSingle},
    {"B06", Icon::Heli2},
    {"B06T", Icon::Heli2},
    {"B1", Icon::B1},
    {"B190", Icon::TwinProp},
    {"B2", Icon::B2},
    {"B212", Icon::Heli2},
    {"B222", Icon::Heli2},
    {"B350", Icon::TwinProp},
    {"B37M", Icon::B737},
    {"B38M", Icon::B737},
    {"B39M", Icon::B737},
    {"B3XM", Icon::B737},
    {"B407", Icon::Heli4},
    {"B412", Icon::Heli4},
    {"B427", Icon::Heli4},
    {"B429", Icon::Heli4},
    {"B430", Icon::Heli4},
    {"B461", Icon::RearJet},
    {"B462", Icon::RearJet},
    {"B463", Icon::RearJet},
    {"B47G", Icon::Heli2},
    {"B505", Icon::Heli2},
    {"B52", Icon::B52},
    {"B609", Icon::Tiltrotor},
    {"B701", Icon::B707},
    {"B703", Icon::B707},
    {"B712", Icon::RearJet},
    {"B720", Icon::B707},
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
    {"BE19", Icon::SingleLow},
    {"BE20", Icon::TwinProp},
    {"BE23", Icon::SingleLow},
    {"BE24", Icon::SingleLow},
    {"BE30", Icon::TwinProp},
    {"BE33", Icon::SingleLow},
    {"BE35", Icon::SingleLow},
    {"BE36", Icon::SingleLow},
    {"BE40", Icon::BizJet},
    {"BE55", Icon::TwinPiston},
    {"BE58", Icon::TwinPiston},
    {"BE60", Icon::TwinPiston},
    {"BE76", Icon::TwinPiston},
    {"BE95", Icon::TwinPiston},
    {"BE99", Icon::TwinTurboprop},
    {"BE9L", Icon::TwinProp},
    {"BE9T", Icon::TwinProp},
    {"BK17", Icon::Heli4},
    {"BL8", Icon::SingleHigh},
    {"BLCF", Icon::B747},
    {"BN2P", Icon::TwinHigh},
    {"BN2T", Icon::TwinHigh},
    {"C130", Icon::C130},
    {"C135", Icon::B707},
    {"C150", Icon::SingleHigh},
    {"C152", Icon::SingleHigh},
    {"C160", Icon::C130},
    {"C162", Icon::SingleHigh},
    {"C17", Icon::Freighter},
    {"C170", Icon::SingleHigh},
    {"C172", Icon::SingleHigh},
    {"C175", Icon::SingleHigh},
    {"C177", Icon::SingleHigh},
    {"C180", Icon::SingleHigh},
    {"C182", Icon::SingleHigh},
    {"C185", Icon::SingleHigh},
    {"C195", Icon::SingleHigh},
    {"C2", Icon::Hawkeye},
    {"C205", Icon::SingleHigh},
    {"C206", Icon::SingleHigh},
    {"C207", Icon::SingleHigh},
    {"C208", Icon::TurbopropSingleHigh},
    {"C210", Icon::SingleHigh},
    {"C212", Icon::TwinHigh},
    {"C25A", Icon::BizJet},
    {"C25B", Icon::BizJet},
    {"C25C", Icon::BizJet},
    {"C25M", Icon::BizJet},
    {"C27J", Icon::C130},
    {"C295", Icon::Turboprop},
    {"C303", Icon::TwinPiston},
    {"C30J", Icon::C130},
    {"C310", Icon::TwinPiston},
    {"C320", Icon::TwinPiston},
    {"C335", Icon::TwinPiston},
    {"C340", Icon::TwinPiston},
    {"C401", Icon::TwinPiston},
    {"C402", Icon::TwinPiston},
    {"C411", Icon::TwinPiston},
    {"C414", Icon::TwinPiston},
    {"C42", Icon::SingleHigh},
    {"C421", Icon::TwinPiston},
    {"C425", Icon::TwinTurboprop},
    {"C441", Icon::TwinTurboprop},
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
    {"C72R", Icon::SingleHigh},
    {"C750", Icon::BizJet},
    {"C77R", Icon::SingleHigh},
    {"C82R", Icon::SingleHigh},
    {"C82S", Icon::SingleHigh},
    {"C82T", Icon::SingleHigh},
    {"C919", Icon::A320},
    {"CH7A", Icon::SingleHigh},
    {"CL2P", Icon::Amphibian},
    {"CL2T", Icon::Amphibian},
    {"CL30", Icon::BigBizJet},
    {"CL35", Icon::BigBizJet},
    {"CL4T", Icon::Amphibian},
    {"CL60", Icon::BigBizJet},
    {"CN35", Icon::Turboprop},
    {"COL3", Icon::SingleLow},
    {"COL4", Icon::SingleLow},
    {"CRJ1", Icon::CRJ},
    {"CRJ2", Icon::CRJ},
    {"CRJ7", Icon::CRJ},
    {"CRJ9", Icon::CRJ},
    {"CRJX", Icon::CRJ},
    {"CRUZ", Icon::SingleLow},
    {"D228", Icon::TwinHigh},
    {"D328", Icon::Turboprop},
    {"DA20", Icon::SingleLow},
    {"DA40", Icon::SingleLow},
    {"DA42", Icon::TwinPiston},
    {"DA62", Icon::TwinPiston},
    {"DC10", Icon::MD11},
    {"DC91", Icon::RearJet},
    {"DC92", Icon::RearJet},
    {"DC93", Icon::RearJet},
    {"DC94", Icon::RearJet},
    {"DC95", Icon::RearJet},
    {"DG1T", Icon::Glider},
    {"DH3T", Icon::TurbopropSingleHigh},
    {"DH8A", Icon::Turboprop},
    {"DH8B", Icon::Turboprop},
    {"DH8C", Icon::Turboprop},
    {"DH8D", Icon::Turboprop},
    {"DHC2", Icon::TurbopropSingleHigh},
    {"DHC3", Icon::TurbopropSingleHigh},
    {"DHC6", Icon::TwinHigh},
    {"DHC7", Icon::Turboprop},
    {"DISC", Icon::Glider},
    {"DO28", Icon::TwinHigh},
    {"DR40", Icon::SingleLow},
    {"DUOD", Icon::Glider},
    {"DV20", Icon::SingleLow},
    {"E120", Icon::TwinTurboprop},
    {"E121", Icon::TwinTurboprop},
    {"E135", Icon::ERJ},
    {"E145", Icon::ERJ},
    {"E170", Icon::E195},
    {"E175", Icon::E195},
    {"E190", Icon::E195},
    {"E195", Icon::E195},
    {"E2", Icon::Hawkeye},
    {"E290", Icon::E195},
    {"E295", Icon::E195},
    {"E314", Icon::TurbopropSingle},
    {"E35L", Icon::ERJ},
    {"E3CF", Icon::Awacs},
    {"E3TF", Icon::Awacs},
    {"E45X", Icon::ERJ},
    {"E50P", Icon::BizJet},
    {"E545", Icon::BizJet},
    {"E550", Icon::BizJet},
    {"E55P", Icon::BizJet},
    {"E6", Icon::B707},
    {"E737", Icon::B737},
    {"E75L", Icon::E195},
    {"E75S", Icon::E195},
    {"E767", Icon::Awacs},
    {"EA18", Icon::FighterTwin},
    {"EC20", Icon::Heli3},
    {"EC25", Icon::Heli5},
    {"EC30", Icon::Heli3},
    {"EC35", Icon::Heli4},
    {"EC45", Icon::Heli4},
    {"EC55", Icon::Heli5},
    {"EC75", Icon::Heli5},
    {"EH10", Icon::Heli5},
    {"EN28", Icon::Heli3},
    {"EN48", Icon::Heli3},
    {"EUFI", Icon::FighterDelta},
    {"F100", Icon::RearJet},
    {"F14", Icon::FighterTwin},
    {"F15", Icon::FighterTwin},
    {"F16", Icon::FighterSingle},
    {"F18", Icon::FighterTwin},
    {"F18S", Icon::FighterTwin},
    {"F2", Icon::FighterSingle},
    {"F22", Icon::FighterTwin},
    {"F260", Icon::SingleLow},
    {"F27", Icon::Turboprop},
    {"F2TH", Icon::Falcon},
    {"F35", Icon::FighterTwin},
    {"F4", Icon::FighterSingle},
    {"F406", Icon::TwinTurboprop},
    {"F5", Icon::FighterSingle},
    {"F50", Icon::Turboprop},
    {"F70", Icon::RearJet},
    {"F900", Icon::Falcon},
    {"FA10", Icon::Falcon},
    {"FA20", Icon::Falcon},
    {"FA50", Icon::Falcon},
    {"FA6X", Icon::Falcon},
    {"FA7X", Icon::Falcon},
    {"FA8X", Icon::Falcon},
    {"G115", Icon::SingleLow},
    {"G280", Icon::BigBizJet},
    {"GA5C", Icon::BigBizJet},
    {"GA6C", Icon::BigBizJet},
    {"GA7C", Icon::BigBizJet},
    {"GA8", Icon::SingleHigh},
    {"GA8C", Icon::BigBizJet},
    {"GALX", Icon::BigBizJet},
    {"GAZL", Icon::Heli3},
    {"GL5T", Icon::BigBizJet},
    {"GL7T", Icon::BigBizJet},
    {"GL8T", Icon::BigBizJet},
    {"GLEX", Icon::BigBizJet},
    {"GLF2", Icon::BigBizJet},
    {"GLF3", Icon::BigBizJet},
    {"GLF4", Icon::BigBizJet},
    {"GLF5", Icon::BigBizJet},
    {"GLF6", Icon::BigBizJet},
    {"GLID", Icon::Glider},
    {"GRIF", Icon::FighterDelta},
    {"H1", Icon::HeliAttack},
    {"H160", Icon::Heli5},
    {"H25A", Icon::BizJet},
    {"H25B", Icon::BizJet},
    {"H25C", Icon::BizJet},
    {"H269", Icon::Heli3},
    {"H46", Icon::HeliTandem},
    {"H47", Icon::HeliTandem},
    {"H500", Icon::Heli5},
    {"H53", Icon::HeliLarge},
    {"H53S", Icon::HeliLarge},
    {"H60", Icon::HeliLarge},
    {"H64", Icon::HeliAttack},
    {"HAWK", Icon::JetTrainer},
    {"HDJT", Icon::BizJet},
    {"HRON", Icon::Reaper},
    {"IL14", Icon::Turboprop},
    {"IL18", Icon::C130},
    {"IL62", Icon::RearJet},
    {"IL76", Icon::Freighter},
    {"J10", Icon::FighterDelta},
    {"J11", Icon::FighterTwin},
    {"J15", Icon::FighterTwin},
    {"J20", Icon::FighterTwin},
    {"J3", Icon::SingleHigh},
    {"J328", Icon::ERJ},
    {"JANU", Icon::Glider},
    {"JF17", Icon::FighterSingle},
    {"JS31", Icon::TwinTurboprop},
    {"JS32", Icon::TwinTurboprop},
    {"JS41", Icon::TwinTurboprop},
    {"K35E", Icon::B707},
    {"K35R", Icon::B707},
    {"K35T", Icon::B707},
    {"K46", Icon::B767},
    {"K8", Icon::JetTrainer},
    {"KA26", Icon::HeliCoax},
    {"KA27", Icon::HeliCoax},
    {"KA32", Icon::HeliCoax},
    {"KA50", Icon::HeliCoax},
    {"KA52", Icon::HeliCoax},
    {"KC46", Icon::B767},
    {"KFIR", Icon::FighterDelta},
    {"KODI", Icon::TurbopropSingleHigh},
    {"L101", Icon::MD11},
    {"L159", Icon::JetTrainer},
    {"L188", Icon::C130},
    {"L39", Icon::JetTrainer},
    {"L410", Icon::TwinHigh},
    {"LAMA", Icon::Heli3},
    {"LANC", Icon::SingleLow},
    {"LJ31", Icon::BizJet},
    {"LJ35", Icon::BizJet},
    {"LJ40", Icon::BizJet},
    {"LJ45", Icon::BizJet},
    {"LJ55", Icon::BizJet},
    {"LJ60", Icon::BizJet},
    {"LJ70", Icon::BizJet},
    {"LJ75", Icon::BizJet},
    {"LS8", Icon::Glider},
    {"M20J", Icon::SingleLow},
    {"M20K", Icon::SingleLow},
    {"M20P", Icon::SingleLow},
    {"M20R", Icon::SingleLow},
    {"M20T", Icon::SingleLow},
    {"M28", Icon::TwinHigh},
    {"M346", Icon::JetTrainer},
    {"M5", Icon::SingleHigh},
    {"M6", Icon::SingleHigh},
    {"M7", Icon::SingleHigh},
    {"MA60", Icon::Turboprop},
    {"MD11", Icon::MD11},
    {"MD60", Icon::Heli4},
    {"MD81", Icon::RearJet},
    {"MD82", Icon::RearJet},
    {"MD83", Icon::RearJet},
    {"MD87", Icon::RearJet},
    {"MD88", Icon::RearJet},
    {"MD90", Icon::RearJet},
    {"MG21", Icon::FighterSingle},
    {"MG29", Icon::FighterTwin},
    {"MG31", Icon::FighterTwin},
    {"MI14", Icon::Heli5},
    {"MI17", Icon::Heli5},
    {"MI24", Icon::HeliAttack},
    {"MI26", Icon::HeliLarge},
    {"MI28", Icon::HeliAttack},
    {"MI8", Icon::Heli5},
    {"MIR2", Icon::FighterDelta},
    {"MQ1", Icon::Reaper},
    {"MQ9", Icon::Reaper},
    {"MT7", Icon::SingleHigh},
    {"MU2", Icon::TwinTurboprop},
    {"NH90", Icon::HeliLarge},
    {"NIMB", Icon::Glider},
    {"P180", Icon::TwinTurboprop},
    {"P210", Icon::SingleHigh},
    {"P28A", Icon::SingleLow},
    {"P28B", Icon::SingleLow},
    {"P28R", Icon::SingleLow},
    {"P28T", Icon::SingleLow},
    {"P28U", Icon::SingleLow},
    {"P3", Icon::C130},
    {"P32R", Icon::SingleLow},
    {"P32T", Icon::SingleLow},
    {"P46T", Icon::TurbopropSingle},
    {"P68", Icon::TwinHigh},
    {"P8", Icon::B737},
    {"PA12", Icon::SingleHigh},
    {"PA14", Icon::SingleHigh},
    {"PA18", Icon::SingleHigh},
    {"PA20", Icon::SingleHigh},
    {"PA22", Icon::SingleHigh},
    {"PA23", Icon::TwinPiston},
    {"PA24", Icon::SingleLow},
    {"PA27", Icon::TwinPiston},
    {"PA28", Icon::SingleLow},
    {"PA30", Icon::TwinPiston},
    {"PA31", Icon::TwinPiston},
    {"PA32", Icon::SingleLow},
    {"PA34", Icon::TwinPiston},
    {"PA38", Icon::SingleLow},
    {"PA42", Icon::TwinTurboprop},
    {"PA44", Icon::TwinPiston},
    {"PA46", Icon::TurbopropSingle},
    {"PAY1", Icon::TwinTurboprop},
    {"PAY2", Icon::TwinTurboprop},
    {"PAY3", Icon::TwinTurboprop},
    {"PAY4", Icon::TwinTurboprop},
    {"PBY", Icon::Amphibian},
    {"PC12", Icon::TurbopropSingle},
    {"PC21", Icon::TurbopropSingle},
    {"PC24", Icon::BizJet},
    {"PC6P", Icon::TurbopropSingleHigh},
    {"PC6T", Icon::TurbopropSingleHigh},
    {"PC7", Icon::TurbopropSingle},
    {"PC9", Icon::TurbopropSingle},
    {"PRM1", Icon::BizJet},
    {"Q1", Icon::Reaper},
    {"Q4", Icon::Reaper},
    {"Q9", Icon::Reaper},
    {"R135", Icon::B707},
    {"R22", Icon::Heli2},
    {"R44", Icon::Heli2},
    {"R66", Icon::Heli2},
    {"RFAL", Icon::FighterDelta},
    {"RJ1H", Icon::RearJet},
    {"RJ70", Icon::RearJet},
    {"RJ85", Icon::RearJet},
    {"RQ4", Icon::Reaper},
    {"RV10", Icon::SingleLow},
    {"RV12", Icon::SingleLow},
    {"RV14", Icon::SingleLow},
    {"RV4", Icon::SingleLow},
    {"RV6", Icon::SingleLow},
    {"RV7", Icon::SingleLow},
    {"RV8", Icon::SingleLow},
    {"RV9", Icon::SingleLow},
    {"S22T", Icon::SingleLow},
    {"S61", Icon::Heli5},
    {"S70", Icon::HeliLarge},
    {"S76", Icon::Heli4},
    {"S92", Icon::HeliLarge},
    {"SAVG", Icon::SingleHigh},
    {"SB20", Icon::Turboprop},
    {"SC7", Icon::TwinHigh},
    {"SF34", Icon::Turboprop},
    {"SF50", Icon::BizJet},
    {"SH33", Icon::Turboprop},
    {"SH36", Icon::Turboprop},
    {"SHIP", Icon::Airship},
    {"SR20", Icon::SingleLow},
    {"SR22", Icon::SingleLow},
    {"SU25", Icon::FighterSingle},
    {"SU27", Icon::FighterTwin},
    {"SU30", Icon::FighterTwin},
    {"SU33", Icon::FighterTwin},
    {"SU34", Icon::FighterTwin},
    {"SU35", Icon::FighterTwin},
    {"SU57", Icon::FighterTwin},
    {"SU95", Icon::E195},
    {"SW2", Icon::TwinTurboprop},
    {"SW3", Icon::TwinTurboprop},
    {"SW4", Icon::TwinTurboprop},
    {"T154", Icon::RearJet},
    {"T210", Icon::SingleHigh},
    {"T34", Icon::TurbopropSingle},
    {"T38", Icon::JetTrainer},
    {"T45", Icon::JetTrainer},
    {"T50", Icon::JetTrainer},
    {"T6", Icon::TurbopropSingle},
    {"T7", Icon::JetTrainer},
    {"TB10", Icon::SingleLow},
    {"TB20", Icon::SingleLow},
    {"TB21", Icon::SingleLow},
    {"TB9", Icon::SingleLow},
    {"TBM7", Icon::TurbopropSingle},
    {"TBM8", Icon::TurbopropSingle},
    {"TBM9", Icon::TurbopropSingle},
    {"TEX2", Icon::TurbopropSingle},
    {"TIGR", Icon::HeliAttack},
    {"TOBA", Icon::SingleLow},
    {"TOR", Icon::FighterSingle},
    {"UH1", Icon::Heli2},
    {"ULAC", Icon::Ultralight},
    {"V22", Icon::Tiltrotor},
    {"VENT", Icon::Glider},
    {"YK13", Icon::JetTrainer},
    {"YK42", Icon::RearJet},
};

struct IconInfo {
  const char* mask;
  uint8_t size;
};

const IconInfo kInfo[] = {
    {"a0", 40},  // Generic
    {"g_single_high", 30},  // Light
    {"a2", 34},  // Small
    {"a3", 42},  // Large
    {"a4", 46},  // HighVortex
    {"a5", 48},  // Heavy
    {"a6", 36},  // HighPerf
    {"a7", 38},  // Rotor
    {"b0", 22},  // Drone
    {"b1", 32},  // Glider
    {"b2", 30},  // Airship
    {"b3", 20},  // Parachute
    {"b4", 28},  // Ultralight
    {"c0", 18},  // Ground
    {"a320", 42},  // A320
    {"a330", 48},  // A330
    {"a340", 50},  // A340
    {"a380", 56},  // A380
    {"b737", 42},  // B737
    {"b747", 54},  // B747
    {"b767", 46},  // B767
    {"b777", 50},  // B777
    {"b787", 48},  // B787
    {"md11", 48},  // MD11
    {"f100", 40},  // RearJet
    {"crjx", 38},  // CRJ
    {"erj", 38},  // ERJ
    {"e195", 40},  // E195
    {"dh8a", 40},  // Turboprop
    {"beechcraft", 36},  // TwinProp
    {"learjet", 34},  // BizJet
    {"glf5", 38},  // BigBizJet
    {"fa7x", 38},  // Falcon
    {"c130", 44},  // C130
    {"heavyfreighter", 54},  // Freighter
    {"tiltrotorcraft", 40},  // Tiltrotor
    {"amphibian", 36},  // Amphibian
    {"g_single_high", 30},  // SingleHigh
    {"g_single_low", 30},  // SingleLow
    {"g_turboprop_single", 34},  // TurbopropSingle
    {"g_turboprop_single_high", 34},  // TurbopropSingleHigh
    {"g_twin_piston", 32},  // TwinPiston
    {"g_twin_turboprop", 36},  // TwinTurboprop
    {"g_twin_high", 38},  // TwinHigh
    {"g_b707", 48},  // B707
    {"g_awacs", 50},  // Awacs
    {"g_hawkeye", 40},  // Hawkeye
    {"g_b52", 56},  // B52
    {"g_b1", 50},  // B1
    {"g_b2", 54},  // B2
    {"g_mq9", 32},  // Reaper
    {"g_fighter_single", 34},  // FighterSingle
    {"g_fighter_twin", 38},  // FighterTwin
    {"g_fighter_delta", 36},  // FighterDelta
    {"g_attack", 38},  // Attack
    {"g_jet_trainer", 32},  // JetTrainer
    {"g_heli_2", 32},  // Heli2
    {"g_heli_3", 34},  // Heli3
    {"g_heli_4", 36},  // Heli4
    {"g_heli_5", 40},  // Heli5
    {"g_heli_large", 42},  // HeliLarge
    {"g_heli_tandem", 44},  // HeliTandem
    {"g_heli_attack", 38},  // HeliAttack
    {"g_heli_coax", 38},  // HeliCoax
};
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
