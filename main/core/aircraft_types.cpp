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
    {"A002", Icon::X_GYRO},  // by class
    {"A1", Icon::X_A1SKY},
    {"A10", Icon::X_A10},
    {"A109", Icon::X_A109},
    {"A119", Icon::X_A109},
    {"A122", Icon::Light},  // by class
    {"A124", Icon::X_AN124},
    {"A129", Icon::X_A129},
    {"A139", Icon::X_A139},
    {"A140", Icon::Turboprop},  // by class
    {"A148", Icon::BigBizJet},  // by class
    {"A149", Icon::X_A189},
    {"A158", Icon::BigBizJet},  // by class
    {"A159", Icon::X_LYNX},
    {"A16", Icon::Light},  // by class
    {"A169", Icon::X_A169},
    {"A178", Icon::BigBizJet},  // by class
    {"A189", Icon::X_A189},
    {"A19", Icon::Light},  // by class
    {"A19N", Icon::X_A319},
    {"A20", Icon::TwinPiston},  // by class
    {"A205", Icon::X_GYRO},  // by class
    {"A20N", Icon::X_A320},
    {"A21", Icon::Light},  // by class
    {"A210", Icon::X_RV12},
    {"A211", Icon::Light},  // by class
    {"A21N", Icon::X_A321},
    {"A22", Icon::Light},  // by class
    {"A223", Icon::Light},  // by class
    {"A225", Icon::X_AN225},
    {"A23", Icon::Light},  // by class
    {"A25", Icon::X_LA4},  // by class
    {"A251", Icon::X_G21},  // by class
    {"A26", Icon::X_A26},
    {"A27", Icon::Light},  // by class
    {"A270", Icon::TurbopropSingle},  // by class
    {"A29", Icon::Light},  // by class
    {"A2RT", Icon::Heli4},  // by class
    {"A3", Icon::BigBizJet},  // by class
    {"A306", Icon::X_A300},
    {"A30B", Icon::X_A300},
    {"A31", Icon::Light},  // by class
    {"A310", Icon::X_A310},
    {"A318", Icon::X_A318},
    {"A319", Icon::X_A319},
    {"A320", Icon::X_A320},
    {"A321", Icon::X_A321},
    {"A33", Icon::Light},  // by class
    {"A332", Icon::X_A332},
    {"A333", Icon::X_A333},
    {"A337", Icon::X_BELUGAXL},
    {"A338", Icon::X_A339},
    {"A339", Icon::X_A339},
    {"A33E", Icon::X_GLIDER},
    {"A33P", Icon::X_GLIDER},
    {"A342", Icon::X_A343},
    {"A343", Icon::X_A343},
    {"A345", Icon::X_A346},
    {"A346", Icon::X_A346},
    {"A35", Icon::Light},  // by class
    {"A359", Icon::X_A359},
    {"A35K", Icon::X_A35K},
    {"A37", Icon::X_A37},
    {"A388", Icon::X_A388},
    {"A3ST", Icon::X_BELUGA},
    {"A4", Icon::X_A4},
    {"A400", Icon::X_A400},
    {"A411", Icon::TwinPiston},  // by class
    {"A5", Icon::X_ICONA5},
    {"A50", Icon::X_IL76},
    {"A500", Icon::TwinPiston},  // by class
    {"A504", Icon::Light},  // by class
    {"A6", Icon::X_A6},
    {"A600", Icon::X_R22},
    {"A660", Icon::X_AT8T},
    {"A6M", Icon::X_ZERO},
    {"A700", Icon::BizJet},  // by class
    {"A743", Icon::X_AN72},
    {"A748", Icon::Turboprop},  // by class
    {"A890", Icon::Light},  // by class
    {"A9", Icon::X_PA25},
    {"A900", Icon::Light},  // by class
    {"A910", Icon::Light},  // by class
    {"AA1", Icon::X_AA5},
    {"AA37", Icon::TwinPiston},  // by class
    {"AA5", Icon::X_AA5},
    {"AAT3", Icon::Light},  // by class
    {"AAT4", Icon::Light},  // by class
    {"AB11", Icon::Light},  // by class
    {"AB15", Icon::Light},  // by class
    {"AB18", Icon::Light},  // by class
    {"AB95", Icon::Light},  // by class
    {"AC10", Icon::X_GYRO},
    {"AC11", Icon::X_PA24},
    {"AC31", Icon::Heli3},  // by class
    {"AC33", Icon::Heli5},  // by class
    {"AC4", Icon::Light},  // by class
    {"AC50", Icon::X_AC50},
    {"AC52", Icon::TwinPiston},  // by class
    {"AC56", Icon::X_AC50},
    {"AC5A", Icon::Light},  // by class
    {"AC68", Icon::X_AC50},
    {"AC6L", Icon::X_AC50},
    {"AC72", Icon::TwinPiston},  // by class
    {"AC80", Icon::X_AC50},
    {"AC90", Icon::X_AC50},
    {"AC95", Icon::X_AC50},
    {"ACAM", Icon::X_AIRCAM},
    {"ACAR", Icon::Light},  // by class
    {"ACED", Icon::Light},  // by class
    {"ACJR", Icon::X_LA4},  // by class
    {"ACPL", Icon::Light},  // by class
    {"ACR2", Icon::Light},  // by class
    {"ACRD", Icon::TwinPiston},  // by class
    {"ACRO", Icon::Light},  // by class
    {"ACSR", Icon::Light},  // by class
    {"AD20", Icon::Light},  // by class
    {"ADEL", Icon::X_GYRO},  // by class
    {"ADVE", Icon::Light},  // by class
    {"ADVN", Icon::X_LA4},  // by class
    {"AE45", Icon::TwinPiston},  // by class
    {"AEA1", Icon::Light},  // by class
    {"AERK", Icon::Light},  // by class
    {"AEST", Icon::X_AEST},
    {"AFOX", Icon::Light},  // by class
    {"AG02", Icon::Light},  // by class
    {"AG04", Icon::Light},  // by class
    {"AG1", Icon::X_GYRO},
    {"AG10", Icon::Light},  // by class
    {"AG14", Icon::Light},  // by class
    {"AG60", Icon::Amphibian},  // by class
    {"AGSH", Icon::X_LA4},  // by class
    {"AH1", Icon::X_AH1},
    {"AH64", Icon::X_H64},
    {"AI10", Icon::Light},  // by class
    {"AIGT", Icon::Light},  // by class
    {"AIRD", Icon::Light},  // by class
    {"AIRL", Icon::TwinPiston},  // by class
    {"AJ27", Icon::X_ARJ21},
    {"AJET", Icon::X_AJET},
    {"AK1", Icon::Light},  // by class
    {"AKIN", Icon::X_TB2},
    {"AKNC", Icon::TwinTurboprop},  // by class
    {"AKOY", Icon::X_LA4},  // by class
    {"AKRO", Icon::Light},  // by class
    {"ALBU", Icon::X_KITFOX},
    {"ALC1", Icon::Light},  // by class
    {"ALGR", Icon::Light},  // by class
    {"ALH", Icon::Heli4},  // by class
    {"ALIG", Icon::Light},  // by class
    {"ALIZ", Icon::TurbopropSingle},  // by class
    {"ALO2", Icon::X_ALO3},
    {"ALO3", Icon::X_ALO3},
    {"ALPI", Icon::Light},  // by class
    {"ALSL", Icon::Light},  // by class
    {"ALTO", Icon::Light},  // by class
    {"AM3", Icon::Light},  // by class
    {"AMX", Icon::FighterSingle},
    {"AN12", Icon::X_C130},
    {"AN2", Icon::X_AN2},
    {"AN22", Icon::Freighter},
    {"AN24", Icon::X_AN26},
    {"AN26", Icon::X_AN26},
    {"AN28", Icon::TwinHigh},
    {"AN3", Icon::TurbopropSingle},  // by class
    {"AN30", Icon::Turboprop},  // by class
    {"AN32", Icon::X_AN26},
    {"AN38", Icon::Turboprop},  // by class
    {"AN70", Icon::C130},  // by class
    {"AN72", Icon::X_AN72},
    {"AN8", Icon::Turboprop},  // by class
    {"ANDR", Icon::Light},  // by class
    {"ANGL", Icon::TwinPiston},  // by class
    {"ANKA", Icon::Light},  // by class
    {"ANSN", Icon::TwinPiston},  // by class
    {"ANST", Icon::Heli4},  // by class
    {"AP20", Icon::Light},  // by class
    {"AP22", Icon::X_KITFOX},
    {"AP24", Icon::X_LA4},  // by class
    {"AP26", Icon::TwinPiston},  // by class
    {"AP28", Icon::TwinPiston},  // by class
    {"AP32", Icon::Light},  // by class
    {"AP36", Icon::TwinPiston},  // by class
    {"APM2", Icon::Light},  // by class
    {"APM3", Icon::Light},  // by class
    {"APM4", Icon::Light},  // by class
    {"APUP", Icon::Light},  // by class
    {"AR1", Icon::X_GYRO},
    {"AR11", Icon::X_J3},
    {"AR15", Icon::X_C180},
    {"AR50", Icon::Light},  // by class
    {"AR5T", Icon::Light},  // by class
    {"AR65", Icon::X_J3},
    {"AR6T", Icon::X_J3},
    {"AR79", Icon::Light},  // by class
    {"ARCP", Icon::X_GLIDER2},
    {"ARCU", Icon::X_GLIDER2},
    {"ARES", Icon::JetTrainer},  // by class
    {"ARKS", Icon::Light},  // by class
    {"ARON", Icon::Light},  // by class
    {"ARV1", Icon::Light},  // by class
    {"ARVA", Icon::TwinTurboprop},  // by class
    {"ARWF", Icon::Light},  // by class
    {"AS02", Icon::X_G115},
    {"AS13", Icon::X_GLIDER2},
    {"AS14", Icon::X_MOTORGLIDER},
    {"AS16", Icon::X_MOTORGLIDER},
    {"AS20", Icon::X_GLIDER},
    {"AS21", Icon::X_GLIDER2},
    {"AS22", Icon::X_GLIDER_OPEN},
    {"AS25", Icon::X_GLIDER_OPEN},
    {"AS27", Icon::X_GLIDER},
    {"AS28", Icon::X_GLIDER},
    {"AS29", Icon::X_GLIDER},
    {"AS2T", Icon::TurbopropSingle},  // by class
    {"AS31", Icon::X_GLIDER_OPEN},
    {"AS32", Icon::X_EC25},
    {"AS3B", Icon::X_EC25},
    {"AS50", Icon::X_AS50},
    {"AS55", Icon::X_AS55},
    {"AS65", Icon::X_AS65},
    {"AS80", Icon::Light},  // by class
    {"ASDR", Icon::Light},  // by class
    {"ASK21", Icon::X_GLIDER2},
    {"ASK3", Icon::X_GLIDER},
    {"ASO4", Icon::Light},  // by class
    {"ASO5", Icon::Light},  // by class
    {"ASOX", Icon::Light},  // by class
    {"ASTO", Icon::X_RV12},
    {"ASTR", Icon::X_G150},
    {"ASW2", Icon::X_GLIDER},
    {"AT01", Icon::X_AT8T},
    {"AT1", Icon::Light},  // by class
    {"AT2P", Icon::Light},  // by class
    {"AT3", Icon::BigBizJet},  // by class
    {"AT3P", Icon::X_AT8T},
    {"AT3T", Icon::X_AT8T},
    {"AT43", Icon::X_ATR42},
    {"AT44", Icon::X_ATR42},
    {"AT45", Icon::X_ATR42},
    {"AT46", Icon::X_ATR42},
    {"AT4T", Icon::X_AT8T},
    {"AT5P", Icon::Light},  // by class
    {"AT5T", Icon::X_AT8T},
    {"AT6T", Icon::X_AT8T},
    {"AT72", Icon::X_ATR72},
    {"AT73", Icon::X_ATR72},
    {"AT75", Icon::X_ATR72},
    {"AT76", Icon::X_ATR72},
    {"AT8T", Icon::X_AT8T},
    {"ATAC", Icon::Light},  // by class
    {"ATG1", Icon::BizJet},  // by class
    {"ATIS", Icon::Light},  // by class
    {"ATL", Icon::Light},  // by class
    {"ATLA", Icon::Turboprop},  // by class
    {"ATP", Icon::Turboprop},  // by class
    {"ATTL", Icon::Light},  // by class
    {"AU11", Icon::Light},  // by class
    {"AUJ2", Icon::Light},  // by class
    {"AUJ4", Icon::Light},  // by class
    {"AURA", Icon::X_RV12},
    {"AUS3", Icon::Light},  // by class
    {"AUS4", Icon::Light},  // by class
    {"AUS5", Icon::Light},  // by class
    {"AUS6", Icon::Light},  // by class
    {"AUS7", Icon::Light},  // by class
    {"AUS9", Icon::Light},  // by class
    {"AV68", Icon::Light},  // by class
    {"AV8B", Icon::X_AV8B},
    {"AVAM", Icon::X_LA4},  // by class
    {"AVID", Icon::X_KITFOX},
    {"AVIN", Icon::Light},  // by class
    {"AVK4", Icon::TwinTurboprop},  // by class
    {"AVLN", Icon::X_LA4},  // by class
    {"AVTR", Icon::X_LA4},  // by class
    {"AXE", Icon::X_DC6},  // by class
    {"B06", Icon::X_B06},
    {"B06T", Icon::X_B06},
    {"B1", Icon::B1},
    {"B103", Icon::X_G21},  // by class
    {"B105", Icon::X_EC35},
    {"B14A", Icon::X_PA24},
    {"B14B", Icon::X_PA24},
    {"B14C", Icon::X_PA24},
    {"B150", Icon::Heli3},  // by class
    {"B17", Icon::X_B17},
    {"B18T", Icon::X_BE18},
    {"B190", Icon::X_B190},
    {"B2", Icon::B2},
    {"B204", Icon::X_UH1},
    {"B205", Icon::X_UH1},
    {"B209", Icon::Light},  // by class
    {"B21", Icon::Heavy},  // by class
    {"B212", Icon::X_UH1},
    {"B214", Icon::Heli3},  // by class
    {"B222", Icon::X_B222},
    {"B23", Icon::TwinPiston},  // by class
    {"B230", Icon::X_B222},
    {"B23E", Icon::Light},  // by class
    {"B24", Icon::X_B24},
    {"B25", Icon::X_B25},
    {"B26", Icon::X_A26},
    {"B26M", Icon::X_B26M},
    {"B29", Icon::X_B29},
    {"B305", Icon::Heli2},  // by class
    {"B350", Icon::X_B350},
    {"B360", Icon::Light},  // by class
    {"B36T", Icon::X_BE36},
    {"B377", Icon::X_C97},
    {"B37M", Icon::X_B737},
    {"B38M", Icon::X_B738},
    {"B39M", Icon::X_B739},
    {"B3XM", Icon::X_B739},
    {"B407", Icon::X_B407},
    {"B412", Icon::X_B412},
    {"B427", Icon::X_B429},
    {"B429", Icon::X_B429},
    {"B430", Icon::Heli4},
    {"B461", Icon::X_BAE146},
    {"B462", Icon::X_BAE146},
    {"B463", Icon::X_BAE146_300},
    {"B47G", Icon::X_B47G},
    {"B47J", Icon::Heli2},  // by class
    {"B47T", Icon::Heli3},  // by class
    {"B505", Icon::X_B505},
    {"B52", Icon::B52},
    {"B525", Icon::X_B525},
    {"B58T", Icon::X_BE58},
    {"B60", Icon::Light},  // by class
    {"B609", Icon::Tiltrotor},
    {"B60T", Icon::TwinTurboprop},  // by class
    {"B701", Icon::X_KC135},
    {"B703", Icon::X_KC135},
    {"B712", Icon::X_B712},
    {"B720", Icon::X_KC135},
    {"B721", Icon::X_B721},
    {"B722", Icon::X_B722},
    {"B731", Icon::X_B732},
    {"B732", Icon::X_B732},
    {"B733", Icon::X_B733},
    {"B734", Icon::X_B734},
    {"B735", Icon::X_B735},
    {"B736", Icon::X_B736},
    {"B737", Icon::X_B737},
    {"B738", Icon::X_B738},
    {"B739", Icon::X_B739},
    {"B741", Icon::X_B744},
    {"B742", Icon::X_B744},
    {"B743", Icon::X_B744},
    {"B744", Icon::X_B744},
    {"B748", Icon::X_B748},
    {"B74D", Icon::X_B744},
    {"B74R", Icon::X_B744},
    {"B74S", Icon::X_B74S},
    {"B752", Icon::X_B752},
    {"B753", Icon::X_B753},
    {"B762", Icon::X_B762},
    {"B763", Icon::X_B763},
    {"B764", Icon::X_B764},
    {"B772", Icon::X_B772},
    {"B773", Icon::X_B773},
    {"B778", Icon::X_B779},
    {"B779", Icon::X_B779},
    {"B77L", Icon::X_B772},
    {"B77W", Icon::X_B77W},
    {"B788", Icon::X_B788},
    {"B789", Icon::X_B789},
    {"B78X", Icon::X_B78X},
    {"BA11", Icon::X_DC9},
    {"BABY", Icon::Heli2},  // by class
    {"BALL", Icon::X_BALLOON},
    {"BAR6", Icon::Light},  // by class
    {"BARC", Icon::Light},  // by class
    {"BASS", Icon::TwinPiston},  // by class
    {"BBAT", Icon::Light},  // by class
    {"BBIR", Icon::Light},  // by class
    {"BCA3", Icon::Light},  // by class
    {"BCAT", Icon::X_BCAT},
    {"BCMP", Icon::Light},  // by class
    {"BCS1", Icon::X_A220_100},
    {"BCS3", Icon::X_A220_300},
    {"BD10", Icon::JetTrainer},  // by class
    {"BD12", Icon::Light},  // by class
    {"BD17", Icon::Light},  // by class
    {"BD4", Icon::Light},  // by class
    {"BD5", Icon::Light},  // by class
    {"BD5J", Icon::JetTrainer},  // by class
    {"BD5T", Icon::TurbopropSingle},  // by class
    {"BDOG", Icon::X_G115},
    {"BE10", Icon::X_BE9L},
    {"BE12", Icon::X_G21},  // by class
    {"BE17", Icon::X_BE17},
    {"BE18", Icon::X_BE18},
    {"BE19", Icon::SingleLow},
    {"BE20", Icon::X_BE20},
    {"BE22", Icon::TurbopropSingle},  // by class
    {"BE23", Icon::SingleLow},
    {"BE24", Icon::SingleLow},
    {"BE30", Icon::X_B350},
    {"BE32", Icon::Turboprop},  // by class
    {"BE33", Icon::X_BE36},
    {"BE35", Icon::X_BE35},
    {"BE36", Icon::X_BE36},
    {"BE40", Icon::X_BE40},
    {"BE4W", Icon::BigBizJet},  // by class
    {"BE50", Icon::X_BE58},
    {"BE55", Icon::X_BE58},
    {"BE56", Icon::TwinPiston},  // by class
    {"BE58", Icon::X_BE58},
    {"BE60", Icon::TwinPiston},
    {"BE65", Icon::X_C414},
    {"BE70", Icon::TwinPiston},  // by class
    {"BE76", Icon::X_BE76},
    {"BE77", Icon::X_PA38},
    {"BE80", Icon::TwinPiston},  // by class
    {"BE88", Icon::TwinPiston},  // by class
    {"BE95", Icon::X_BE58},
    {"BE99", Icon::TwinTurboprop},
    {"BE9L", Icon::X_BE9L},
    {"BE9T", Icon::X_BE9L},
    {"BEA5", Icon::Light},  // by class
    {"BEAR", Icon::X_KITFOX},
    {"BELF", Icon::C130},  // by class
    {"BER2", Icon::X_BE200},
    {"BER4", Icon::X_G21},  // by class
    {"BETA", Icon::Light},  // by class
    {"BEVR", Icon::Light},  // by class
    {"BF19", Icon::Light},  // by class
    {"BFIT", Icon::Light},  // by class
    {"BILO", Icon::Light},  // by class
    {"BIPL", Icon::Light},  // by class
    {"BIRD", Icon::Light},  // by class
    {"BISC", Icon::Light},  // by class
    {"BK17", Icon::X_EC45},
    {"BKUT", Icon::Light},  // by class
    {"BL11", Icon::Light},  // by class
    {"BL17", Icon::X_PA24},
    {"BL19", Icon::X_PA24},
    {"BL8", Icon::X_BL8},
    {"BLAN", Icon::X_BLANIK},
    {"BLBU", Icon::Light},  // by class
    {"BLCF", Icon::X_BLCF},
    {"BLEN", Icon::TwinPiston},  // by class
    {"BLKS", Icon::X_SPORTBIP},
    {"BLSA", Icon::Light},  // by class
    {"BM6", Icon::Light},  // by class
    {"BMAN", Icon::Light},  // by class
    {"BN2P", Icon::X_BN2},
    {"BN2T", Icon::X_BN2},
    {"BO40", Icon::Light},  // by class
    {"BOBC", Icon::Light},  // by class
    {"BOLT", Icon::X_SPORTBIP},
    {"BOOM", Icon::TwinPiston},  // by class
    {"BPAT", Icon::Light},  // by class
    {"BPOD", Icon::X_DC6},  // by class
    {"BPUM", Icon::Light},  // by class
    {"BR14", Icon::Light},  // by class
    {"BR23", Icon::X_RV12},
    {"BR54", Icon::X_GYRO},  // by class
    {"BR60", Icon::Light},  // by class
    {"BR61", Icon::Light},  // by class
    {"BR8", Icon::Light},  // by class
    {"BRAV", Icon::X_KITFOX},
    {"BRB2", Icon::X_H269},
    {"BREZ", Icon::X_RV12},
    {"BROU", Icon::Light},  // by class
    {"BS60", Icon::TwinTurboprop},  // by class
    {"BSTP", Icon::Heli4},  // by class
    {"BSTR", Icon::Light},  // by class
    {"BSWN", Icon::Light},  // by class
    {"BT36", Icon::X_BE36},
    {"BT7", Icon::JetTrainer},  // by class
    {"BTB2", Icon::X_TB2},
    {"BTB3", Icon::Light},  // by class
    {"BTUB", Icon::Light},  // by class
    {"BU20", Icon::X_TRIMOTOR},  // by class
    {"BU31", Icon::X_SPORTBIP},
    {"BU33", Icon::X_SPORTBIP},
    {"BU81", Icon::Light},  // by class
    {"BUC", Icon::BigBizJet},  // by class
    {"BUCA", Icon::X_LA4},  // by class
    {"BULT", Icon::Light},  // by class
    {"BUSH", Icon::Light},  // by class
    {"BW60", Icon::Light},  // by class
    {"BW6T", Icon::Light},  // by class
    {"BX2", Icon::Light},  // by class
    {"C02T", Icon::TwinTurboprop},  // by class
    {"C04T", Icon::TwinTurboprop},  // by class
    {"C06T", Icon::TurbopropSingle},  // by class
    {"C07T", Icon::TurbopropSingle},  // by class
    {"C08T", Icon::C130},  // by class
    {"C1", Icon::BigBizJet},  // by class
    {"C101", Icon::JetTrainer},  // by class
    {"C10T", Icon::TurbopropSingle},  // by class
    {"C119", Icon::X_C119},
    {"C120", Icon::X_C152},
    {"C123", Icon::X_DHC4},
    {"C125", Icon::X_TRIMOTOR},  // by class
    {"C130", Icon::X_C130},
    {"C135", Icon::X_KC135},
    {"C140", Icon::X_C152},
    {"C141", Icon::X_C141},
    {"C14T", Icon::TwinTurboprop},  // by class
    {"C15", Icon::X_BAE146},  // by class
    {"C150", Icon::X_C152},
    {"C152", Icon::X_C152},
    {"C160", Icon::X_C160},
    {"C162", Icon::SingleHigh},
    {"C17", Icon::X_C17},
    {"C170", Icon::X_C180},
    {"C172", Icon::X_C172},
    {"C175", Icon::SingleHigh},
    {"C177", Icon::X_C177},
    {"C180", Icon::X_C180},
    {"C182", Icon::X_C182},
    {"C185", Icon::X_C180},
    {"C188", Icon::X_PA25},
    {"C190", Icon::Light},  // by class
    {"C195", Icon::X_C180},
    {"C2", Icon::Hawkeye},
    {"C205", Icon::X_C182},
    {"C206", Icon::X_C182},
    {"C207", Icon::X_C182},
    {"C208", Icon::X_C208},
    {"C210", Icon::X_C210},
    {"C212", Icon::X_C212},
    {"C21T", Icon::TwinTurboprop},  // by class
    {"C22J", Icon::BizJet},  // by class
    {"C240", Icon::X_SR22},
    {"C25A", Icon::X_C25A},
    {"C25B", Icon::X_C25B},
    {"C25C", Icon::X_C25C},
    {"C25M", Icon::X_C525},
    {"C270", Icon::Light},  // by class
    {"C27J", Icon::X_C27J},
    {"C295", Icon::X_C295},
    {"C303", Icon::X_C310},
    {"C306", Icon::Light},  // by class
    {"C309", Icon::Light},  // by class
    {"C30J", Icon::X_C130},
    {"C310", Icon::X_C310},
    {"C311", Icon::Light},  // by class
    {"C320", Icon::X_C310},
    {"C335", Icon::X_C340},
    {"C336", Icon::TwinPiston},  // by class
    {"C337", Icon::X_C337},
    {"C340", Icon::X_C340},
    {"C365", Icon::TurbopropSingle},  // by class
    {"C390", Icon::X_C390},
    {"C401", Icon::X_C414},
    {"C402", Icon::X_C414},
    {"C404", Icon::X_C421},
    {"C408", Icon::X_C408},
    {"C411", Icon::X_C414},
    {"C414", Icon::X_C414},
    {"C42", Icon::X_KITFOX},
    {"C421", Icon::X_C421},
    {"C425", Icon::X_C441},
    {"C441", Icon::X_C441},
    {"C46", Icon::X_C46},
    {"C47", Icon::X_DC3},
    {"C5", Icon::X_C5},
    {"C500", Icon::X_C500},
    {"C501", Icon::X_C500},
    {"C510", Icon::X_C510},
    {"C525", Icon::X_C525},
    {"C526", Icon::X_C525},
    {"C550", Icon::X_C500},
    {"C551", Icon::X_C500},
    {"C55B", Icon::X_C500},
    {"C560", Icon::X_C560},
    {"C56X", Icon::X_C56X},
    {"C5M", Icon::X_C5},
    {"C650", Icon::X_C650},
    {"C680", Icon::X_C680},
    {"C68A", Icon::X_C68A},
    {"C700", Icon::X_C700},
    {"C72R", Icon::X_C172},
    {"C750", Icon::X_C750},
    {"C77R", Icon::X_C177},
    {"C82", Icon::TwinPiston},  // by class
    {"C82R", Icon::X_C182},
    {"C82S", Icon::X_C182},
    {"C82T", Icon::X_C182},
    {"C919", Icon::X_C919},
    {"C97", Icon::X_C97},
    {"CA12", Icon::TurbopropSingle},  // by class
    {"CA19", Icon::Light},  // by class
    {"CA1P", Icon::Light},  // by class
    {"CA1T", Icon::TurbopropSingle},  // by class
    {"CA25", Icon::Light},  // by class
    {"CA3", Icon::Light},  // by class
    {"CA4", Icon::Light},  // by class
    {"CA41", Icon::Light},  // by class
    {"CA6", Icon::Light},  // by class
    {"CA61", Icon::Light},  // by class
    {"CA65", Icon::Light},  // by class
    {"CA7P", Icon::Light},  // by class
    {"CA7T", Icon::TurbopropSingle},  // by class
    {"CA8", Icon::TurbopropSingle},  // by class
    {"CA9", Icon::TurbopropSingle},  // by class
    {"CABI", Icon::TurbopropSingle},  // by class
    {"CABN", Icon::Light},  // by class
    {"CAD2", Icon::Light},  // by class
    {"CAD4", Icon::Light},  // by class
    {"CAJ", Icon::JetTrainer},  // by class
    {"CAML", Icon::X_CAMEL},
    {"CAMP", Icon::X_J3},
    {"CAN4", Icon::Light},  // by class
    {"CAPL", Icon::Light},  // by class
    {"CAR", Icon::Light},  // by class
    {"CARV", Icon::X_DC6},  // by class
    {"CASS", Icon::Light},  // by class
    {"CAT", Icon::X_PBY},
    {"CAT1", Icon::Light},  // by class
    {"CAT2", Icon::TwinPiston},  // by class
    {"CAW", Icon::Light},  // by class
    {"CB1", Icon::X_SPORTBIP},
    {"CC11", Icon::X_XCUB},
    {"CC18", Icon::X_XCUB},
    {"CC19", Icon::X_XCUB},
    {"CD2", Icon::X_G21},  // by class
    {"CDC6", Icon::Light},  // by class
    {"CDUS", Icon::X_GYRO_CABIN},
    {"CDW1", Icon::Light},  // by class
    {"CE15", Icon::Light},  // by class
    {"CE22", Icon::X_G21},  // by class
    {"CE23", Icon::X_LA4},  // by class
    {"CE25", Icon::X_G21},  // by class
    {"CE27", Icon::X_G21},  // by class
    {"CE43", Icon::Light},  // by class
    {"CEGL", Icon::Light},  // by class
    {"CEL5", Icon::Light},  // by class
    {"CELR", Icon::Light},  // by class
    {"CENT", Icon::Light},  // by class
    {"CFRE", Icon::X_LA4},  // by class
    {"CG3", Icon::Light},  // by class
    {"CGAN", Icon::Light},  // by class
    {"CH1", Icon::TurbopropSingle},  // by class
    {"CH10", Icon::Light},  // by class
    {"CH12", Icon::Heli2},  // by class
    {"CH14", Icon::Heli3},  // by class
    {"CH15", Icon::Light},  // by class
    {"CH18", Icon::Light},  // by class
    {"CH20", Icon::Light},  // by class
    {"CH25", Icon::Light},  // by class
    {"CH2T", Icon::Light},  // by class
    {"CH3", Icon::Light},  // by class
    {"CH30", Icon::Light},  // by class
    {"CH40", Icon::TwinPiston},  // by class
    {"CH47", Icon::X_H47},
    {"CH50", Icon::Light},  // by class
    {"CH53", Icon::X_H53},
    {"CH60", Icon::X_KITFOX},
    {"CH62", Icon::TwinPiston},  // by class
    {"CH64", Icon::Light},  // by class
    {"CH65", Icon::Light},  // by class
    {"CH7", Icon::Heli2},  // by class
    {"CH70", Icon::X_KITFOX},
    {"CH75", Icon::X_KITFOX},
    {"CH7A", Icon::X_BL8},
    {"CH7B", Icon::X_BL8},
    {"CH80", Icon::Light},  // by class
    {"CHAL", Icon::X_RS12},
    {"CHAN", Icon::Light},  // by class
    {"CHCS", Icon::Light},  // by class
    {"CHGO", Icon::Light},  // by class
    {"CHIC", Icon::Light},  // by class
    {"CHIF", Icon::Heli2},  // by class
    {"CHIN", Icon::X_KITFOX},
    {"CHIP", Icon::Light},  // by class
    {"CHP2", Icon::Light},  // by class
    {"CHR1", Icon::Light},  // by class
    {"CHR4", Icon::Light},  // by class
    {"CHSY", Icon::X_GYRO},  // by class
    {"CICA", Icon::TwinPiston},  // by class
    {"CJ1", Icon::Light},  // by class
    {"CJ6", Icon::X_YK52},
    {"CKUO", Icon::BigBizJet},  // by class
    {"CL2P", Icon::X_CL415},
    {"CL2T", Icon::X_CL415},
    {"CL30", Icon::X_CL30},
    {"CL35", Icon::X_CL30},
    {"CL41", Icon::JetTrainer},  // by class
    {"CL4G", Icon::X_SGUP},
    {"CL4T", Icon::X_CL415},
    {"CL60", Icon::X_CL60},
    {"CL8", Icon::Light},  // by class
    {"CLA", Icon::Light},  // by class
    {"CLB1", Icon::Light},  // by class
    {"CLBR", Icon::Light},  // by class
    {"CLD2", Icon::X_GYRO},
    {"CLDS", Icon::Light},  // by class
    {"CLON", Icon::X_GYRO_CABIN},
    {"CMA3", Icon::Light},  // by class
    {"CMAS", Icon::Light},  // by class
    {"CMD1", Icon::X_GYRO},
    {"CMDE", Icon::X_GYRO},
    {"CMDT", Icon::X_GYRO},
    {"CN12", Icon::Light},  // by class
    {"CN35", Icon::X_CN235},
    {"CNBR", Icon::BigBizJet},  // by class
    {"CNDR", Icon::TwinTurboprop},  // by class
    {"CNGP", Icon::Light},  // by class
    {"CNUK", Icon::Light},  // by class
    {"CO50", Icon::Light},  // by class
    {"COAR", Icon::Light},  // by class
    {"COBR", Icon::Light},  // by class
    {"COL3", Icon::SingleLow},
    {"COL4", Icon::SingleLow},
    {"COLT", Icon::Light},  // by class
    {"COMU", Icon::Heli2},  // by class
    {"CONI", Icon::X_CONI},
    {"COOT", Icon::X_LA4},  // by class
    {"CORO", Icon::Light},  // by class
    {"CORR", Icon::Light},  // by class
    {"CORS", Icon::X_CORS},
    {"CORV", Icon::TwinPiston},  // by class
    {"COUG", Icon::X_SPORTBIP},
    {"COUR", Icon::X_M7},
    {"COY2", Icon::X_KITFOX},
    {"COZJ", Icon::JetTrainer},  // by class
    {"COZY", Icon::X_LGEZ},
    {"CP10", Icon::X_CP23},
    {"CP13", Icon::Light},  // by class
    {"CP20", Icon::Light},  // by class
    {"CP21", Icon::X_CP23},
    {"CP22", Icon::Light},  // by class
    {"CP23", Icon::X_CP23},
    {"CP30", Icon::Light},  // by class
    {"CP32", Icon::Light},  // by class
    {"CP60", Icon::Light},  // by class
    {"CP65", Icon::Light},  // by class
    {"CP75", Icon::Light},  // by class
    {"CP80", Icon::Light},  // by class
    {"CP90", Icon::Light},  // by class
    {"CPNA", Icon::Light},  // by class
    {"CPUP", Icon::Light},  // by class
    {"CR10", Icon::Light},  // by class
    {"CRA1", Icon::Light},  // by class
    {"CRAC", Icon::Light},  // by class
    {"CRBN", Icon::Light},  // by class
    {"CRER", Icon::X_KITFOX},
    {"CRES", Icon::X_P750},
    {"CRIO", Icon::X_LA4},  // by class
    {"CRJ1", Icon::X_CRJ2},
    {"CRJ2", Icon::X_CRJ2},
    {"CRJ7", Icon::X_CRJ7},
    {"CRJ9", Icon::X_CRJ9},
    {"CRJX", Icon::X_CRJ9},
    {"CRUZ", Icon::SingleLow},
    {"CT4", Icon::X_T34},
    {"CTAH", Icon::Light},  // by class
    {"CTLN", Icon::X_LA4},  // by class
    {"CUB2", Icon::X_J3},
    {"CUCA", Icon::Light},  // by class
    {"CUDA", Icon::Light},  // by class
    {"CULP", Icon::Light},  // by class
    {"CULV", Icon::Light},  // by class
    {"CULX", Icon::TwinPiston},  // by class
    {"CVLP", Icon::X_DC3},
    {"CVLT", Icon::Turboprop},  // by class
    {"CX5", Icon::Light},  // by class
    {"CYCL", Icon::Light},  // by class
    {"CYGT", Icon::Light},  // by class
    {"D1", Icon::TwinPiston},  // by class
    {"D11", Icon::X_DR40},
    {"D139", Icon::Light},  // by class
    {"D140", Icon::Light},  // by class
    {"D150", Icon::Light},  // by class
    {"D18", Icon::Light},  // by class
    {"D201", Icon::Light},  // by class
    {"D21", Icon::Light},  // by class
    {"D228", Icon::X_D228},
    {"D24", Icon::Light},  // by class
    {"D25", Icon::Light},  // by class
    {"D250", Icon::Light},  // by class
    {"D253", Icon::Light},  // by class
    {"D28D", Icon::TwinPiston},  // by class
    {"D28T", Icon::TwinTurboprop},  // by class
    {"D31", Icon::Light},  // by class
    {"D328", Icon::X_D328},
    {"D39", Icon::Light},  // by class
    {"D4", Icon::Light},  // by class
    {"D5", Icon::Light},  // by class
    {"D5TU", Icon::Light},  // by class
    {"D6", Icon::Light},  // by class
    {"D6CR", Icon::Light},  // by class
    {"D7", Icon::X_CAMEL},
    {"D8", Icon::Light},  // by class
    {"DA2", Icon::Light},  // by class
    {"DA20", Icon::X_DA20},
    {"DA36", Icon::Light},  // by class
    {"DA40", Icon::X_DA40},
    {"DA42", Icon::X_DA42},
    {"DA5", Icon::Light},  // by class
    {"DA50", Icon::X_DA40},
    {"DA62", Icon::X_DA62},
    {"DAHU", Icon::Light},  // by class
    {"DAKH", Icon::X_KITFOX},
    {"DAL1", Icon::Light},  // by class
    {"DAL4", Icon::Light},  // by class
    {"DAL5", Icon::Light},  // by class
    {"DART", Icon::Light},  // by class
    {"DC10", Icon::X_DC10},
    {"DC2", Icon::X_DC3},
    {"DC3", Icon::X_DC3},
    {"DC3S", Icon::X_DC3},
    {"DC3T", Icon::X_DC3},
    {"DC4", Icon::X_DC4},
    {"DC6", Icon::X_DC6},
    {"DC7", Icon::X_DC6},
    {"DC81", Icon::X_DC8},
    {"DC82", Icon::X_DC8},
    {"DC83", Icon::X_DC8},
    {"DC85", Icon::X_DC8},
    {"DC86", Icon::X_DC8},
    {"DC87", Icon::X_DC8},
    {"DC91", Icon::X_DC9},
    {"DC92", Icon::X_DC9},
    {"DC93", Icon::X_DC9},
    {"DC94", Icon::X_DC9},
    {"DC95", Icon::X_DC9},
    {"DDUC", Icon::Light},  // by class
    {"DEAG", Icon::X_GYRO},
    {"DEFI", Icon::X_LGEZ},
    {"DELF", Icon::Light},  // by class
    {"DF10", Icon::Light},  // by class
    {"DF1M", Icon::Light},  // by class
    {"DF2", Icon::Light},  // by class
    {"DF3", Icon::Light},  // by class
    {"DFL6", Icon::Light},  // by class
    {"DFLY", Icon::X_DFLY},
    {"DG15", Icon::Light},  // by class
    {"DG1T", Icon::X_GLIDER},
    {"DG30", Icon::X_GLIDER},
    {"DG80", Icon::X_GLIDER},
    {"DH2T", Icon::TurbopropSingle},  // by class
    {"DH3T", Icon::X_DHC3},
    {"DH4T", Icon::Turboprop},  // by class
    {"DH60", Icon::Light},  // by class
    {"DH80", Icon::Light},  // by class
    {"DH82", Icon::X_DH82},
    {"DH83", Icon::Light},  // by class
    {"DH84", Icon::TwinPiston},  // by class
    {"DH85", Icon::Light},  // by class
    {"DH87", Icon::Light},  // by class
    {"DH88", Icon::TwinPiston},  // by class
    {"DH89", Icon::TwinPiston},  // by class
    {"DH8A", Icon::X_DH8A},
    {"DH8B", Icon::X_DH8A},
    {"DH8C", Icon::X_DH8C},
    {"DH8D", Icon::X_DH8D},
    {"DH90", Icon::TwinPiston},  // by class
    {"DH94", Icon::Light},  // by class
    {"DHA3", Icon::X_TRIMOTOR},  // by class
    {"DHC1", Icon::X_DHC1},
    {"DHC2", Icon::X_DHC2},
    {"DHC3", Icon::X_DHC3},
    {"DHC4", Icon::X_DHC4},
    {"DHC5", Icon::X_DHC4},
    {"DHC6", Icon::X_DHC6},
    {"DHC7", Icon::X_DHC7},
    {"DIES", Icon::Light},  // by class
    {"DIJ3", Icon::Light},  // by class
    {"DIJ4", Icon::X_LA4},  // by class
    {"DIMO", Icon::X_MOTORGLIDER},
    {"DINO", Icon::Light},  // by class
    {"DIPR", Icon::X_LA4},  // by class
    {"DISC", Icon::X_GLIDER},
    {"DJET", Icon::JetTrainer},  // by class
    {"DJIN", Icon::Heli3},  // by class
    {"DNGO", Icon::X_LA4},  // by class
    {"DO27", Icon::Light},  // by class
    {"DO28", Icon::TwinHigh},
    {"DOCX", Icon::Light},  // by class
    {"DON", Icon::Light},  // by class
    {"DOVE", Icon::TwinPiston},  // by class
    {"DR1", Icon::X_DR1},
    {"DR10", Icon::X_DR40},
    {"DR22", Icon::X_DR40},
    {"DR30", Icon::Light},  // by class
    {"DR40", Icon::X_DR40},
    {"DRAG", Icon::Heli2},  // by class
    {"DRAK", Icon::X_DRAKEN},
    {"DRIF", Icon::Light},  // by class
    {"DRON", Icon::X_QUAD},
    {"DRTG", Icon::Light},  // by class
    {"DSA1", Icon::X_SPORTBIP},
    {"DSK", Icon::Light},  // by class
    {"DSLK", Icon::Light},  // by class
    {"DT45", Icon::TurbopropSingle},  // by class
    {"DTA1", Icon::Light},  // by class
    {"DTA2", Icon::Light},  // by class
    {"DUB2", Icon::Light},  // by class
    {"DUCE", Icon::Light},  // by class
    {"DUOD", Icon::X_GLIDER2},
    {"DUR5", Icon::Light},  // by class
    {"DV1", Icon::Light},  // by class
    {"DV2", Icon::Light},  // by class
    {"DV20", Icon::X_DA20},
    {"DW1", Icon::X_PA24},
    {"DWD2", Icon::Light},  // by class
    {"DYH2", Icon::Heli2},  // by class
    {"DYH3", Icon::Heli2},  // by class
    {"E110", Icon::X_JS31},
    {"E120", Icon::X_E120},
    {"E121", Icon::TwinTurboprop},
    {"E135", Icon::X_E135},
    {"E145", Icon::X_E145},
    {"E170", Icon::X_E170},
    {"E175", Icon::X_E175},
    {"E190", Icon::X_E190},
    {"E195", Icon::X_E195},
    {"E2", Icon::Hawkeye},
    {"E200", Icon::X_E300},
    {"E230", Icon::X_E300},
    {"E275", Icon::BigBizJet},  // by class
    {"E290", Icon::X_E290},
    {"E295", Icon::X_E295},
    {"E2CB", Icon::X_J3},
    {"E300", Icon::X_E300},
    {"E314", Icon::TurbopropSingle},
    {"E330", Icon::X_E300},
    {"E350", Icon::TurbopropSingle},  // by class
    {"E35L", Icon::X_E135},
    {"E390", Icon::X_C390},
    {"E3CF", Icon::Awacs},
    {"E3TF", Icon::Awacs},
    {"E400", Icon::X_E300},
    {"E45X", Icon::X_E145},
    {"E500", Icon::X_E300},
    {"E50P", Icon::X_E50P},
    {"E530", Icon::BigBizJet},  // by class
    {"E545", Icon::X_E545},
    {"E550", Icon::X_E550},
    {"E55P", Icon::X_E55P},
    {"E6", Icon::X_KC135},
    {"E737", Icon::X_B737},
    {"E75L", Icon::X_E175},
    {"E75S", Icon::X_E175},
    {"E767", Icon::Awacs},
    {"E7BH", Icon::Light},  // by class
    {"EA18", Icon::X_F18},
    {"EA40", Icon::JetTrainer},  // by class
    {"EA50", Icon::X_EA50},
    {"EA6", Icon::X_A6},
    {"EAEA", Icon::Light},  // by class
    {"EAGL", Icon::X_SPORTBIP},
    {"EAGX", Icon::Light},  // by class
    {"EBOY", Icon::Light},  // by class
    {"EC20", Icon::X_EC20},
    {"EC25", Icon::X_EC25},
    {"EC30", Icon::X_EC30},
    {"EC35", Icon::X_EC35},
    {"EC45", Icon::X_EC45},
    {"EC55", Icon::X_AS65},
    {"EC6", Icon::Light},  // by class
    {"EC75", Icon::X_EC75},
    {"ECHO", Icon::X_KITFOX},
    {"EDGE", Icon::X_E300},
    {"EDGT", Icon::X_E300},
    {"EF11", Icon::X_F111},
    {"EF2", Icon::Light},  // by class
    {"EFAN", Icon::TwinPiston},  // by class
    {"EFOX", Icon::X_KITFOX},
    {"EFUS", Icon::Light},  // by class
    {"EGL3", Icon::Heli3},  // by class
    {"EGRT", Icon::TurbopropSingle},  // by class
    {"EH10", Icon::X_EH10},
    {"EL10", Icon::X_GYRO},
    {"EL20", Icon::Light},  // by class
    {"ELA7", Icon::X_GYRO},
    {"ELF", Icon::Light},  // by class
    {"ELIT", Icon::BizJet},  // by class
    {"ELPS", Icon::Light},  // by class
    {"ELSP", Icon::Light},  // by class
    {"ELST", Icon::Light},  // by class
    {"ELTO", Icon::Heli2},  // by class
    {"ELTR", Icon::Light},  // by class
    {"EM10", Icon::JetTrainer},  // by class
    {"EM11", Icon::TwinPiston},  // by class
    {"EMER", Icon::Ground},
    {"EN28", Icon::X_EN28},
    {"EN48", Icon::X_EN28},
    {"EP9", Icon::Light},  // by class
    {"EPER", Icon::Light},  // by class
    {"EPIC", Icon::X_TBM9},
    {"EPX1", Icon::Light},  // by class
    {"ERAC", Icon::Light},  // by class
    {"ERCO", Icon::X_ERCO},
    {"ES11", Icon::Heli3},  // by class
    {"ES13", Icon::Light},  // by class
    {"ESCA", Icon::TurbopropSingle},  // by class
    {"ESCP", Icon::Light},  // by class
    {"ESQL", Icon::Light},  // by class
    {"ETA", Icon::X_GLIDER_OPEN},
    {"ETAR", Icon::JetTrainer},  // by class
    {"EUFI", Icon::X_EUFI},
    {"EUPA", Icon::X_RV12},
    {"EURT", Icon::Light},  // by class
    {"EV55", Icon::TwinPiston},  // by class
    {"EV97", Icon::X_RV12},
    {"EVAN", Icon::TwinPiston},  // by class
    {"EVIC", Icon::JetTrainer},  // by class
    {"EVOP", Icon::X_LNC4},
    {"EVOT", Icon::X_LNC4},
    {"EVSS", Icon::X_RV12},
    {"EX5T", Icon::TurbopropSingle},  // by class
    {"EXEC", Icon::X_R22},
    {"EXEJ", Icon::Heli3},  // by class
    {"EXNG", Icon::X_E300},
    {"EXPL", Icon::X_MD900},
    {"EXPR", Icon::Light},  // by class
    {"EZFL", Icon::Light},  // by class
    {"EZFT", Icon::TwinPiston},  // by class
    {"EZHV", Icon::X_T6},
    {"EZIK", Icon::Light},  // by class
    {"EZKC", Icon::Light},  // by class
    {"F1", Icon::BigBizJet},  // by class
    {"F100", Icon::X_F100},
    {"F104", Icon::X_F104},
    {"F106", Icon::JetTrainer},  // by class
    {"F111", Icon::X_F111},
    {"F117", Icon::X_F117},
    {"F13", Icon::Light},  // by class
    {"F14", Icon::X_F14},
    {"F15", Icon::X_F15},
    {"F156", Icon::X_STORCH},
    {"F16", Icon::X_F16},
    {"F16X", Icon::JetTrainer},  // by class
    {"F18", Icon::X_F18},
    {"F18H", Icon::X_F18},
    {"F18S", Icon::X_F18},
    {"F1FV", Icon::Light},  // by class
    {"F2", Icon::X_F16},
    {"F22", Icon::X_F22},
    {"F260", Icon::SingleLow},
    {"F26T", Icon::TurbopropSingle},  // by class
    {"F27", Icon::X_F50},
    {"F28", Icon::BigBizJet},  // by class
    {"F2TH", Icon::X_F2TH},
    {"F30", Icon::Light},  // by class
    {"F35", Icon::X_F35},
    {"F3F", Icon::Light},  // by class
    {"F4", Icon::X_F4},
    {"F402", Icon::TurbopropSingle},  // by class
    {"F406", Icon::TwinTurboprop},
    {"F41E", Icon::TwinPiston},  // by class
    {"F421", Icon::TurbopropSingle},  // by class
    {"F4U", Icon::X_CORS},
    {"F5", Icon::X_F5},
    {"F50", Icon::X_F50},
    {"F5SA", Icon::BigBizJet},  // by class
    {"F60", Icon::Turboprop},  // by class
    {"F600", Icon::TwinTurboprop},  // by class
    {"F7", Icon::X_TRIMOTOR},  // by class
    {"F70", Icon::X_F70},
    {"F8", Icon::X_F8},
    {"F86", Icon::X_F86},
    {"F8L", Icon::Light},  // by class
    {"F900", Icon::X_FA50},
    {"F9F", Icon::X_T33},
    {"FA01", Icon::Light},  // by class
    {"FA02", Icon::Light},  // by class
    {"FA03", Icon::Light},  // by class
    {"FA04", Icon::Light},  // by class
    {"FA10", Icon::X_FA20},
    {"FA11", Icon::Light},  // by class
    {"FA20", Icon::X_FA20},
    {"FA24", Icon::X_C180},
    {"FA50", Icon::X_FA50},
    {"FA62", Icon::X_DHC1},
    {"FA6X", Icon::X_FA6X},
    {"FA7X", Icon::X_FA7X},
    {"FA8X", Icon::X_FA8X},
    {"FAET", Icon::Light},  // by class
    {"FALC", Icon::Light},  // by class
    {"FALM", Icon::Light},  // by class
    {"FANL", Icon::Light},  // by class
    {"FANT", Icon::Light},  // by class
    {"FB1A", Icon::Light},  // by class
    {"FB1B", Icon::Light},  // by class
    {"FB5", Icon::Light},  // by class
    {"FBA2", Icon::Light},  // by class
    {"FBIR", Icon::Light},  // by class
    {"FC1", Icon::JetTrainer},  // by class
    {"FD2E", Icon::Light},  // by class
    {"FDCT", Icon::X_RV12},
    {"FDF2", Icon::Light},  // by class
    {"FDMC", Icon::Light},  // by class
    {"FE51", Icon::Light},  // by class
    {"FEST", Icon::Light},  // by class
    {"FFLO", Icon::X_QUAD},
    {"FFLY", Icon::Light},  // by class
    {"FG01", Icon::Light},  // by class
    {"FGT", Icon::Light},  // by class
    {"FH11", Icon::Heli3},  // by class
    {"FIBO", Icon::Light},  // by class
    {"FIKD", Icon::Light},  // by class
    {"FIKE", Icon::Light},  // by class
    {"FINC", Icon::Light},  // by class
    {"FJ10", Icon::BizJet},  // by class
    {"FJR3", Icon::Light},  // by class
    {"FK12", Icon::Light},  // by class
    {"FK14", Icon::Light},  // by class
    {"FK9", Icon::X_KITFOX},
    {"FL3", Icon::Light},  // by class
    {"FL53", Icon::Light},  // by class
    {"FL54", Icon::Light},  // by class
    {"FL55", Icon::Light},  // by class
    {"FLAM", Icon::TwinPiston},  // by class
    {"FLCO", Icon::Light},  // by class
    {"FLE2", Icon::Light},  // by class
    {"FLE7", Icon::Light},  // by class
    {"FLIZ", Icon::Light},  // by class
    {"FLSH", Icon::Light},  // by class
    {"FLSS", Icon::Light},  // by class
    {"FM25", Icon::Light},  // by class
    {"FMGO", Icon::TwinPiston},  // by class
    {"FN33", Icon::X_LA4},  // by class
    {"FNKB", Icon::Light},  // by class
    {"FOOF", Icon::Light},  // by class
    {"FORT", Icon::Light},  // by class
    {"FOUG", Icon::X_FOUGA},
    {"FOX", Icon::X_KITFOX},
    {"FOXT", Icon::Light},  // by class
    {"FRBD", Icon::Light},  // by class
    {"FREE", Icon::Light},  // by class
    {"FREL", Icon::Heli5},  // by class
    {"FRNT", Icon::Light},  // by class
    {"FRON", Icon::Light},  // by class
    {"FS51", Icon::Light},  // by class
    {"FT30", Icon::Light},  // by class
    {"FU24", Icon::X_P750},
    {"FURY", Icon::Light},  // by class
    {"FUSI", Icon::Light},  // by class
    {"FW02", Icon::X_LA4},  // by class
    {"FW19", Icon::Light},  // by class
    {"FW21", Icon::Light},  // by class
    {"FW44", Icon::Light},  // by class
    {"FW90", Icon::X_FW90},
    {"FWSB", Icon::X_LA4},  // by class
    {"FX1", Icon::Light},  // by class
    {"G1", Icon::Light},  // by class
    {"G103", Icon::X_GLIDER2},
    {"G109", Icon::X_MOTORGLIDER},
    {"G115", Icon::X_G115},
    {"G120", Icon::X_G115},
    {"G12T", Icon::TurbopropSingle},  // by class
    {"G140", Icon::TurbopropSingle},  // by class
    {"G150", Icon::X_G150},
    {"G159", Icon::Turboprop},  // by class
    {"G15T", Icon::Light},  // by class
    {"G160", Icon::TurbopropSingle},  // by class
    {"G164", Icon::X_G164},
    {"G180", Icon::Light},  // by class
    {"G200", Icon::Light},  // by class
    {"G202", Icon::Light},  // by class
    {"G21", Icon::X_G21},
    {"G21M", Icon::Amphibian},  // by class
    {"G21T", Icon::X_G21},  // by class
    {"G222", Icon::Turboprop},  // by class
    {"G250", Icon::BigBizJet},  // by class
    {"G280", Icon::X_G280},
    {"G2CA", Icon::X_G2CA},
    {"G2GL", Icon::JetTrainer},  // by class
    {"G2T1", Icon::X_SPORTBIP},
    {"G3", Icon::X_KITFOX},
    {"G300", Icon::Light},  // by class
    {"G44", Icon::X_G21},
    {"G46", Icon::Light},  // by class
    {"G4SG", Icon::JetTrainer},  // by class
    {"G59", Icon::Light},  // by class
    {"G64T", Icon::TurbopropSingle},  // by class
    {"G70", Icon::Light},  // by class
    {"G73", Icon::X_G21},
    {"G73T", Icon::X_G21},  // by class
    {"G800", Icon::Light},  // by class
    {"G850", Icon::TwinPiston},  // by class
    {"G91", Icon::JetTrainer},  // by class
    {"G96", Icon::X_S2},
    {"G97", Icon::Light},  // by class
    {"GA10", Icon::TurbopropSingle},  // by class
    {"GA20", Icon::Light},  // by class
    {"GA3C", Icon::BigBizJet},  // by class
    {"GA4C", Icon::BigBizJet},  // by class
    {"GA5C", Icon::X_GA7C},
    {"GA6C", Icon::X_GA7C},
    {"GA7", Icon::TwinPiston},  // by class
    {"GA7C", Icon::X_GA7C},
    {"GA8", Icon::X_GA8},
    {"GA8C", Icon::X_GA7C},
    {"GABR", Icon::Light},  // by class
    {"GALX", Icon::X_G150},
    {"GANT", Icon::C130},  // by class
    {"GAUN", Icon::Light},  // by class
    {"GAVI", Icon::Light},  // by class
    {"GAZL", Icon::X_GAZL},
    {"GB1", Icon::X_E300},
    {"GBSP", Icon::Light},  // by class
    {"GC1", Icon::X_AA5},
    {"GDUK", Icon::X_G21},  // by class
    {"GEMI", Icon::TwinPiston},  // by class
    {"GENI", Icon::Light},  // by class
    {"GEPE", Icon::Light},  // by class
    {"GF20", Icon::Light},  // by class
    {"GFLY", Icon::JetTrainer},  // by class
    {"GJ11", Icon::JetTrainer},  // by class
    {"GL5T", Icon::X_GL5T},
    {"GL7T", Icon::X_GL7T},
    {"GL8T", Icon::X_GL7T},
    {"GLAD", Icon::Light},  // by class
    {"GLAS", Icon::X_GLASAIR},
    {"GLEX", Icon::X_GLEX},
    {"GLF2", Icon::X_GLF5},
    {"GLF3", Icon::X_GLF5},
    {"GLF4", Icon::X_GLF4},
    {"GLF5", Icon::X_GLF5},
    {"GLF6", Icon::X_GLF6},
    {"GLID", Icon::X_GLIDER},
    {"GLIM", Icon::X_MOTORGLIDER},
    {"GLSP", Icon::Light},  // by class
    {"GLST", Icon::X_GLASAIR},
    {"GLTU", Icon::TurbopropSingle},  // by class
    {"GM01", Icon::Light},  // by class
    {"GM17", Icon::TurbopropSingle},  // by class
    {"GMGC", Icon::Light},  // by class
    {"GNAT", Icon::JetTrainer},  // by class
    {"GND", Icon::Ground},
    {"GOBU", Icon::X_GYRO},  // by class
    {"GOLF", Icon::X_RV12},
    {"GOOS", Icon::X_LA4},  // by class
    {"GOTR", Icon::Light},  // by class
    {"GP1", Icon::Light},  // by class
    {"GP3", Icon::X_LA4},
    {"GP4", Icon::X_LA4},
    {"GPRO", Icon::Light},  // by class
    {"GR51", Icon::TurbopropSingle},  // by class
    {"GRAF", Icon::Light},  // by class
    {"GRFN", Icon::Light},  // by class
    {"GRIF", Icon::X_GRIPEN},
    {"GRIZ", Icon::TurbopropSingle},  // by class
    {"GSIS", Icon::Light},  // by class
    {"GSPN", Icon::BizJet},  // by class
    {"GUEP", Icon::Light},  // by class
    {"GURI", Icon::Light},  // by class
    {"GX", Icon::Light},  // by class
    {"GY10", Icon::Light},  // by class
    {"GY20", Icon::Light},  // by class
    {"GY30", Icon::Light},  // by class
    {"GY80", Icon::Light},  // by class
    {"GYRO", Icon::X_GYRO},
    {"H1", Icon::X_AH1},
    {"H111", Icon::TwinPiston},  // by class
    {"H125", Icon::X_AS50},
    {"H12T", Icon::Heli3},  // by class
    {"H130", Icon::X_EC30},
    {"H135", Icon::X_EC35},
    {"H145", Icon::X_EC45},
    {"H155", Icon::X_AS65},
    {"H160", Icon::X_H160},
    {"H175", Icon::X_EC75},
    {"H2", Icon::Heli4},  // by class
    {"H202", Icon::Light},  // by class
    {"H204", Icon::Light},  // by class
    {"H207", Icon::Light},  // by class
    {"H21", Icon::Heli2},  // by class
    {"H215", Icon::X_EC25},
    {"H225", Icon::X_EC25},
    {"H25A", Icon::X_H25B},
    {"H25B", Icon::X_H25B},
    {"H25C", Icon::X_H25B},
    {"H269", Icon::X_H269},
    {"H3", Icon::X_S61},
    {"H40", Icon::Light},  // by class
    {"H43A", Icon::Heli2},  // by class
    {"H43B", Icon::Heli3},  // by class
    {"H46", Icon::X_H46},
    {"H47", Icon::X_H47},
    {"H500", Icon::X_H500},
    {"H520", Icon::X_H500},
    {"H53", Icon::X_H53},
    {"H530", Icon::X_H500},
    {"H53S", Icon::X_H53},
    {"H60", Icon::X_H60},
    {"H64", Icon::X_H64},
    {"H92", Icon::X_S92},
    {"HA2", Icon::X_GYRO},  // by class
    {"HA31", Icon::Light},  // by class
    {"HA4T", Icon::X_C680},
    {"HAHU", Icon::Light},  // by class
    {"HANS", Icon::Light},  // by class
    {"HAR", Icon::X_AV8B},
    {"HAR2", Icon::X_AV8B},
    {"HAW3", Icon::X_GYRO},
    {"HAWK", Icon::X_HAWK},
    {"HB21", Icon::Light},  // by class
    {"HB23", Icon::Light},  // by class
    {"HB3", Icon::Light},  // by class
    {"HCAT", Icon::X_HCAT},
    {"HD34", Icon::TwinPiston},  // by class
    {"HDJT", Icon::X_HDJT},
    {"HEAD", Icon::Light},  // by class
    {"HERN", Icon::X_DC6},  // by class
    {"HF20", Icon::BigBizJet},  // by class
    {"HI27", Icon::Light},  // by class
    {"HIGH", Icon::X_KITFOX},
    {"HIND", Icon::Light},  // by class
    {"HK36", Icon::X_MOTORGLIDER},
    {"HL2", Icon::Light},  // by class
    {"HLC4", Icon::Light},  // by class
    {"HLD4", Icon::Light},  // by class
    {"HM38", Icon::Light},  // by class
    {"HN70", Icon::Light},  // by class
    {"HORN", Icon::Light},  // by class
    {"HORZ", Icon::Light},  // by class
    {"HPTR", Icon::X_LA4},  // by class
    {"HPZL", Icon::Light},  // by class
    {"HR10", Icon::Light},  // by class
    {"HR20", Icon::X_DR40},
    {"HRM9", Icon::Light},  // by class
    {"HRNT", Icon::Light},  // by class
    {"HROC", Icon::X_RV7},
    {"HRON", Icon::X_MQ9},
    {"HRYA", Icon::Light},  // by class
    {"HSMT", Icon::Heli2},  // by class
    {"HT16", Icon::JetTrainer},  // by class
    {"HT2", Icon::Light},  // by class
    {"HT32", Icon::Light},  // by class
    {"HT34", Icon::X_T34},
    {"HT36", Icon::JetTrainer},  // by class
    {"HT40", Icon::TurbopropSingle},  // by class
    {"HU1", Icon::Light},  // by class
    {"HU2", Icon::Light},  // by class
    {"HUCO", Icon::X_AH1},
    {"HUML", Icon::Light},  // by class
    {"HUMM", Icon::Light},  // by class
    {"HUNT", Icon::X_HUNT},
    {"HURI", Icon::X_HURI},
    {"HURK", Icon::TurbopropSingle},
    {"HUSK", Icon::X_HUSK},
    {"HW4P", Icon::X_GYRO},
    {"HW4T", Icon::X_GYRO},  // by class
    {"HX2", Icon::Heli3},  // by class
    {"HYPR", Icon::Light},  // by class
    {"I103", Icon::Light},  // by class
    {"I112", Icon::Turboprop},  // by class
    {"I114", Icon::Turboprop},  // by class
    {"I115", Icon::Light},  // by class
    {"I11B", Icon::Light},  // by class
    {"I153", Icon::Light},  // by class
    {"I15B", Icon::Light},  // by class
    {"I16", Icon::Light},  // by class
    {"I22", Icon::JetTrainer},  // by class
    {"I23", Icon::Light},  // by class
    {"I3", Icon::Light},  // by class
    {"I66", Icon::Light},  // by class
    {"I828", Icon::TurbopropSingle},  // by class
    {"IA46", Icon::Light},  // by class
    {"IA50", Icon::Turboprop},  // by class
    {"IA51", Icon::Light},  // by class
    {"IA58", Icon::TwinTurboprop},  // by class
    {"IA63", Icon::JetTrainer},  // by class
    {"IFUR", Icon::Light},  // by class
    {"IL14", Icon::Turboprop},
    {"IL18", Icon::X_C130},
    {"IL28", Icon::BigBizJet},  // by class
    {"IL38", Icon::C130},  // by class
    {"IL62", Icon::X_IL62},
    {"IL76", Icon::X_IL76},
    {"IL86", Icon::Freighter},  // by class
    {"IL96", Icon::Freighter},  // by class
    {"IMPU", Icon::Light},  // by class
    {"INCQ", Icon::Light},  // by class
    {"INEC", Icon::Light},  // by class
    {"INEX", Icon::Light},  // by class
    {"INTG", Icon::Light},  // by class
    {"ION", Icon::Light},  // by class
    {"IP06", Icon::Light},  // by class
    {"IP10", Icon::Light},  // by class
    {"IP26", Icon::Light},  // by class
    {"IP6A", Icon::Light},  // by class
    {"IPAN", Icon::Light},  // by class
    {"IR21", Icon::Light},  // by class
    {"IR22", Icon::Light},  // by class
    {"IR23", Icon::Light},  // by class
    {"IR24", Icon::Light},  // by class
    {"IR25", Icon::TurbopropSingle},  // by class
    {"IR27", Icon::Light},  // by class
    {"IR28", Icon::Light},  // by class
    {"IR31", Icon::Light},  // by class
    {"IR46", Icon::Light},  // by class
    {"IR99", Icon::JetTrainer},  // by class
    {"IRBS", Icon::Light},  // by class
    {"IS2", Icon::Heli2},  // by class
    {"IS28", Icon::Light},  // by class
    {"ISAT", Icon::Light},  // by class
    {"ISPT", Icon::Light},  // by class
    {"J1", Icon::Light},  // by class
    {"J10", Icon::X_J10},
    {"J11", Icon::X_SU27},
    {"J15", Icon::X_SU27},
    {"J177", Icon::Light},  // by class
    {"J2", Icon::X_J3},
    {"J20", Icon::FighterTwin},
    {"J3", Icon::X_J3},
    {"J300", Icon::Light},  // by class
    {"J328", Icon::X_J328},
    {"J4", Icon::X_J3},
    {"J400", Icon::Light},  // by class
    {"J40E", Icon::Light},  // by class
    {"J4B2", Icon::X_GYRO},
    {"J5", Icon::X_J3},
    {"J600", Icon::Light},  // by class
    {"J8A", Icon::BigBizJet},  // by class
    {"J8B", Icon::BigBizJet},  // by class
    {"JAB2", Icon::Light},  // by class
    {"JAB4", Icon::X_KITFOX},
    {"JABI", Icon::Light},  // by class
    {"JACE", Icon::Light},  // by class
    {"JAG2", Icon::Heli3},  // by class
    {"JAGR", Icon::X_JAGUAR},
    {"JAJ5", Icon::Light},  // by class
    {"JAJ6", Icon::Light},  // by class
    {"JANU", Icon::X_GLIDER2},
    {"JARO", Icon::Light},  // by class
    {"JAS3", Icon::X_GRIPEN},
    {"JAS4", Icon::X_JOBY},
    {"JAST", Icon::JetTrainer},  // by class
    {"JB1", Icon::X_LA4},  // by class
    {"JB15", Icon::Light},  // by class
    {"JC01", Icon::Light},  // by class
    {"JC02", Icon::Light},  // by class
    {"JCOM", Icon::BigBizJet},  // by class
    {"JCRU", Icon::TurbopropSingle},  // by class
    {"JD2", Icon::Light},  // by class
    {"JDOE", Icon::Light},  // by class
    {"JE2", Icon::X_GYRO},
    {"JF17", Icon::FighterSingle},
    {"JFOX", Icon::Light},  // by class
    {"JH7", Icon::BigBizJet},  // by class
    {"JK05", Icon::Light},  // by class
    {"JL9", Icon::JetTrainer},  // by class
    {"JN76", Icon::Light},  // by class
    {"JPM1", Icon::Light},  // by class
    {"JPRO", Icon::X_JPRO},
    {"JRC1", Icon::Light},  // by class
    {"JRO", Icon::X_GYRO},
    {"JS1", Icon::TwinTurboprop},  // by class
    {"JS20", Icon::TwinTurboprop},  // by class
    {"JS3", Icon::TwinTurboprop},  // by class
    {"JS31", Icon::X_JS31},
    {"JS32", Icon::X_JS31},
    {"JS3E", Icon::X_GLIDER},
    {"JS3J", Icon::X_GLIDER},
    {"JS41", Icon::X_JS41},
    {"JSQA", Icon::JetTrainer},  // by class
    {"JSX", Icon::JetTrainer},  // by class
    {"JT2", Icon::Light},  // by class
    {"JU52", Icon::X_JU52},
    {"JUN1", Icon::Light},  // by class
    {"JUN2", Icon::Light},  // by class
    {"JUNR", Icon::Light},  // by class
    {"JUPI", Icon::Light},  // by class
    {"K100", Icon::X_KODI},
    {"K126", Icon::Heli3},  // by class
    {"K200", Icon::X_KODI},
    {"K209", Icon::Heli3},  // by class
    {"K226", Icon::Heli4},  // by class
    {"K250", Icon::Light},  // by class
    {"K35E", Icon::X_KC135},
    {"K35R", Icon::X_KC135},
    {"K35T", Icon::X_KC135},
    {"K46", Icon::X_KC46},
    {"K50", Icon::JetTrainer},  // by class
    {"K51", Icon::Light},  // by class
    {"K8", Icon::JetTrainer},
    {"K900", Icon::X_KODI},
    {"KA25", Icon::Heli4},  // by class
    {"KA26", Icon::X_KA32},
    {"KA27", Icon::X_KA32},
    {"KA32", Icon::X_KA32},
    {"KA50", Icon::X_KA52},
    {"KA52", Icon::X_KA52},
    {"KA62", Icon::Heli4},  // by class
    {"KAAN", Icon::BigBizJet},  // by class
    {"KAFI", Icon::Light},  // by class
    {"KAK1", Icon::Light},  // by class
    {"KAK2", Icon::Light},  // by class
    {"KAK3", Icon::Light},  // by class
    {"KAT3", Icon::TurbopropSingle},  // by class
    {"KATB", Icon::TwinPiston},  // by class
    {"KATR", Icon::Light},  // by class
    {"KC2", Icon::Heavy},  // by class
    {"KC46", Icon::X_KC46},
    {"KE3", Icon::Freighter},  // by class
    {"KEHA", Icon::Light},  // by class
    {"KELA", Icon::Light},  // by class
    {"KELD", Icon::Light},  // by class
    {"KERO", Icon::Light},  // by class
    {"KEST", Icon::TurbopropSingle},  // by class
    {"KF21", Icon::BigBizJet},  // by class
    {"KFAB", Icon::Light},  // by class
    {"KFAS", Icon::Light},  // by class
    {"KFIR", Icon::FighterDelta},
    {"KFIS", Icon::X_LA4},  // by class
    {"KFOX", Icon::X_KITFOX},
    {"KH4", Icon::Heli2},  // by class
    {"KIS2", Icon::Light},  // by class
    {"KIS4", Icon::Light},  // by class
    {"KITH", Icon::Light},  // by class
    {"KITI", Icon::Light},  // by class
    {"KK60", Icon::Light},  // by class
    {"KL07", Icon::Light},  // by class
    {"KL10", Icon::Light},  // by class
    {"KL25", Icon::Light},  // by class
    {"KL35", Icon::Light},  // by class
    {"KLBR", Icon::Light},  // by class
    {"KM2", Icon::Light},  // by class
    {"KMAX", Icon::Heli3},  // by class
    {"KNTW", Icon::Light},  // by class
    {"KODI", Icon::X_KODI},
    {"KOLL", Icon::Light},  // by class
    {"KP2", Icon::Light},  // by class
    {"KP5", Icon::Light},  // by class
    {"KR1", Icon::Light},  // by class
    {"KR2", Icon::X_RV12},
    {"KR21", Icon::Light},  // by class
    {"KR30", Icon::Light},  // by class
    {"KR31", Icon::Light},  // by class
    {"KR34", Icon::Light},  // by class
    {"KRAG", Icon::Light},  // by class
    {"KRAH", Icon::Light},  // by class
    {"KRIC", Icon::Light},  // by class
    {"KSTK", Icon::TwinPiston},  // by class
    {"KT1", Icon::TurbopropSingle},  // by class
    {"KTOO", Icon::Light},  // by class
    {"KZ2", Icon::Light},  // by class
    {"KZ3", Icon::Light},  // by class
    {"KZ4", Icon::TwinPiston},  // by class
    {"KZ7", Icon::Light},  // by class
    {"KZ8", Icon::Light},  // by class
    {"KZLA", Icon::JetTrainer},  // by class
    {"L049", Icon::X_CONI},
    {"L10", Icon::X_BE18},
    {"L101", Icon::X_L1011},
    {"L1049", Icon::X_CONI},
    {"L11", Icon::Light},  // by class
    {"L11E", Icon::Light},  // by class
    {"L12", Icon::X_BE18},
    {"L13", Icon::X_BLANIK},
    {"L13M", Icon::X_BLANIK},
    {"L13S", Icon::Light},  // by class
    {"L14", Icon::X_BE18},
    {"L15", Icon::BigBizJet},  // by class
    {"L159", Icon::X_L39},
    {"L18", Icon::X_BE18},
    {"L181", Icon::Light},  // by class
    {"L188", Icon::X_L188},
    {"L200", Icon::TwinPiston},  // by class
    {"L23", Icon::X_BLANIK},
    {"L29", Icon::X_L29},
    {"L29A", Icon::X_BAE146},  // by class
    {"L29B", Icon::X_BAE146},  // by class
    {"L33", Icon::X_GLIDER},
    {"L37", Icon::TwinPiston},  // by class
    {"L380", Icon::Light},  // by class
    {"L39", Icon::X_L39},
    {"L4", Icon::X_G21},  // by class
    {"L40", Icon::Light},  // by class
    {"L410", Icon::X_L410},
    {"L5", Icon::X_C180},
    {"L59", Icon::JetTrainer},  // by class
    {"L6", Icon::X_G21},  // by class
    {"L60", Icon::Light},  // by class
    {"L610", Icon::Turboprop},  // by class
    {"L70", Icon::Light},  // by class
    {"L749", Icon::X_CONI},
    {"L8", Icon::X_J3},
    {"L90", Icon::TurbopropSingle},  // by class
    {"LA25", Icon::X_LA4},
    {"LA4", Icon::X_LA4},
    {"LA60", Icon::Light},  // by class
    {"LA6T", Icon::TurbopropSingle},  // by class
    {"LA8", Icon::X_G21},  // by class
    {"LACO", Icon::Light},  // by class
    {"LAKR", Icon::X_E300},
    {"LAKX", Icon::Light},  // by class
    {"LAMA", Icon::X_ALO3},
    {"LANC", Icon::X_LANC},
    {"LAR1", Icon::JetTrainer},  // by class
    {"LARK", Icon::Light},  // by class
    {"LAST", Icon::Light},  // by class
    {"LBUG", Icon::Light},  // by class
    {"LCA", Icon::JetTrainer},  // by class
    {"LCB", Icon::Light},  // by class
    {"LCH", Icon::Heli4},  // by class
    {"LCR", Icon::Light},  // by class
    {"LEG2", Icon::X_LNC4},
    {"LEGD", Icon::Light},  // by class
    {"LEOP", Icon::BizJet},  // by class
    {"LESP", Icon::X_LNC4},
    {"LEV2", Icon::Light},  // by class
    {"LEV4", Icon::Light},  // by class
    {"LEVI", Icon::Light},  // by class
    {"LGEZ", Icon::X_LGEZ},
    {"LGND", Icon::Light},  // by class
    {"LH10", Icon::Light},  // by class
    {"LIBE", Icon::X_GLIDER},
    {"LION", Icon::Light},  // by class
    {"LJ23", Icon::BizJet},  // by class
    {"LJ24", Icon::X_LJ35},
    {"LJ25", Icon::X_LJ35},
    {"LJ28", Icon::X_LJ35},
    {"LJ31", Icon::X_LJ35},
    {"LJ35", Icon::X_LJ35},
    {"LJ36", Icon::X_LJ35},
    {"LJ40", Icon::X_LJ45},
    {"LJ45", Icon::X_LJ45},
    {"LJ55", Icon::X_LJ60},
    {"LJ60", Icon::X_LJ60},
    {"LJ70", Icon::X_LJ45},
    {"LJ75", Icon::X_LJ45},
    {"LJ85", Icon::BigBizJet},  // by class
    {"LM5", Icon::Light},  // by class
    {"LM5X", Icon::Light},  // by class
    {"LM7", Icon::Light},  // by class
    {"LMC1", Icon::Light},  // by class
    {"LMK1", Icon::Light},  // by class
    {"LN27", Icon::Light},  // by class
    {"LN3", Icon::X_LA4},  // by class
    {"LNC2", Icon::X_LNC4},
    {"LNC4", Icon::X_LNC4},
    {"LNCE", Icon::X_LNC4},
    {"LNP4", Icon::X_LNC4},
    {"LNT4", Icon::TurbopropSingle},  // by class
    {"LOCA", Icon::Light},  // by class
    {"LOVE", Icon::Light},  // by class
    {"LP1", Icon::Light},  // by class
    {"LR2T", Icon::Heli3},  // by class
    {"LS2", Icon::Light},  // by class
    {"LS4", Icon::X_GLIDER},
    {"LS6", Icon::X_GLIDER},
    {"LS8", Icon::X_GLIDER},
    {"LSTR", Icon::Light},  // by class
    {"LTNG", Icon::BigBizJet},  // by class
    {"LUL5", Icon::Light},  // by class
    {"LUL6", Icon::Light},  // by class
    {"LUL7", Icon::Light},  // by class
    {"LUL8", Icon::Light},  // by class
    {"LV51", Icon::Light},  // by class
    {"LW20", Icon::Light},  // by class
    {"LW40", Icon::Light},  // by class
    {"LWIN", Icon::Light},  // by class
    {"LX32", Icon::Light},  // by class
    {"LX34", Icon::Light},  // by class
    {"LXR", Icon::Light},  // by class
    {"LYNX", Icon::X_LYNX},
    {"LYSA", Icon::X_LYSA},
    {"M10", Icon::X_ERCO},
    {"M101", Icon::TurbopropSingle},  // by class
    {"M106", Icon::Light},  // by class
    {"M108", Icon::Light},  // by class
    {"M10R", Icon::X_ERCO},
    {"M110", Icon::Light},  // by class
    {"M15", Icon::JetTrainer},  // by class
    {"M17", Icon::JetTrainer},  // by class
    {"M18", Icon::X_AT8T},
    {"M18T", Icon::TurbopropSingle},  // by class
    {"M1SC", Icon::Light},  // by class
    {"M2", Icon::Light},  // by class
    {"M200", Icon::X_PA24},
    {"M2000", Icon::X_MIR2},
    {"M203", Icon::Light},  // by class
    {"M20J", Icon::X_M20},
    {"M20K", Icon::X_M20},
    {"M20P", Icon::X_M20},
    {"M20R", Icon::X_M20},
    {"M20T", Icon::X_M20},
    {"M21", Icon::Light},  // by class
    {"M212", Icon::Light},  // by class
    {"M22", Icon::Light},  // by class
    {"M24", Icon::Light},  // by class
    {"M26", Icon::Light},  // by class
    {"M28", Icon::TwinHigh},
    {"M2HK", Icon::Light},  // by class
    {"M308", Icon::Light},  // by class
    {"M326", Icon::JetTrainer},  // by class
    {"M339", Icon::JetTrainer},  // by class
    {"M345", Icon::JetTrainer},  // by class
    {"M346", Icon::X_M346},
    {"M360", Icon::Light},  // by class
    {"M36J", Icon::TwinPiston},  // by class
    {"M4", Icon::X_M7},
    {"M404", Icon::X_DC3},
    {"M5", Icon::X_M7},
    {"M55", Icon::BigBizJet},  // by class
    {"M6", Icon::X_M7},
    {"M600", Icon::X_PA46},
    {"M7", Icon::X_M7},
    {"M700", Icon::X_PA46},
    {"M74", Icon::Heli2},  // by class
    {"M7T", Icon::X_M7},
    {"M8", Icon::X_M7},
    {"M9", Icon::X_M7},
    {"MA1", Icon::Light},  // by class
    {"MA5", Icon::Light},  // by class
    {"MA60", Icon::X_AN26},
    {"MA6H", Icon::Turboprop},  // by class
    {"MAGC", Icon::Light},  // by class
    {"MAGI", Icon::Light},  // by class
    {"MAGN", Icon::Light},  // by class
    {"MAJR", Icon::Light},  // by class
    {"MAKO", Icon::Light},  // by class
    {"MAMB", Icon::Light},  // by class
    {"MAME", Icon::Light},  // by class
    {"MARS", Icon::X_DC6},  // by class
    {"MAVR", Icon::Light},  // by class
    {"MB70", Icon::Light},  // by class
    {"MB80", Icon::Light},  // by class
    {"MC01", Icon::Light},  // by class
    {"MC10", Icon::TwinPiston},  // by class
    {"MC23", Icon::BigBizJet},  // by class
    {"MC45", Icon::X_PA24},
    {"MC90", Icon::Light},  // by class
    {"MCOU", Icon::Light},  // by class
    {"MCOY", Icon::Light},  // by class
    {"MCR1", Icon::X_RV12},
    {"MCR4", Icon::Light},  // by class
    {"MCRR", Icon::Light},  // by class
    {"MCUL", Icon::Light},  // by class
    {"MD11", Icon::X_MD11},
    {"MD3", Icon::Light},  // by class
    {"MD3R", Icon::Light},  // by class
    {"MD52", Icon::X_H500},
    {"MD60", Icon::X_MD900},
    {"MD81", Icon::X_MD80},
    {"MD82", Icon::X_MD80},
    {"MD83", Icon::X_MD80},
    {"MD87", Icon::X_MD80},
    {"MD88", Icon::X_MD80},
    {"MD90", Icon::X_MD90},
    {"MD90X", Icon::X_MD900},
    {"ME08", Icon::Light},  // by class
    {"ME09", Icon::X_ME09},
    {"ME62", Icon::X_ME262},
    {"MEAD", Icon::Light},  // by class
    {"MEL2", Icon::Light},  // by class
    {"MERK", Icon::TwinPiston},  // by class
    {"MESS", Icon::Light},  // by class
    {"METR", Icon::BigBizJet},  // by class
    {"MEXP", Icon::Light},  // by class
    {"MF10", Icon::Light},  // by class
    {"MF17", Icon::Light},  // by class
    {"MG15", Icon::X_MG15},
    {"MG17", Icon::X_MG15},
    {"MG19", Icon::BigBizJet},  // by class
    {"MG21", Icon::X_MG21},
    {"MG23", Icon::JetTrainer},  // by class
    {"MG25", Icon::BigBizJet},  // by class
    {"MG29", Icon::X_MG29},
    {"MG31", Icon::FighterTwin},
    {"MG35", Icon::X_MG29},
    {"MG44", Icon::BigBizJet},  // by class
    {"MGAT", Icon::JetTrainer},  // by class
    {"MGIC", Icon::Light},  // by class
    {"MGNM", Icon::Light},  // by class
    {"MGUL", Icon::Light},  // by class
    {"MH02", Icon::BizJet},  // by class
    {"MH20", Icon::Heli4},  // by class
    {"MH46", Icon::Light},  // by class
    {"MH60", Icon::X_H60},
    {"MI10", Icon::Heli4},  // by class
    {"MI14", Icon::X_MI8},
    {"MI17", Icon::X_MI8},
    {"MI2", Icon::Heli4},  // by class
    {"MI24", Icon::X_MI24},
    {"MI26", Icon::X_MI26},
    {"MI28", Icon::X_MI28},
    {"MI34", Icon::Heli2},  // by class
    {"MI35", Icon::X_MI24},
    {"MI38", Icon::Heli4},  // by class
    {"MI4", Icon::Heli2},  // by class
    {"MI6", Icon::Heli4},  // by class
    {"MI8", Icon::X_MI8},
    {"MIDR", Icon::Light},  // by class
    {"MIMP", Icon::Light},  // by class
    {"MIMU", Icon::Light},  // by class
    {"MIR2", Icon::X_MIR2},
    {"MIR3", Icon::X_MIR2},
    {"MIRA", Icon::X_MIR2},
    {"MITE", Icon::Light},  // by class
    {"MJ10", Icon::Light},  // by class
    {"MJ12", Icon::Light},  // by class
    {"MJ1H", Icon::Light},  // by class
    {"MJ2", Icon::Light},  // by class
    {"MJ3", Icon::Light},  // by class
    {"MJ4", Icon::Light},  // by class
    {"MJ5", Icon::Light},  // by class
    {"MJ51", Icon::Light},  // by class
    {"MJ53", Icon::Light},  // by class
    {"MJ55", Icon::Light},  // by class
    {"MJ7", Icon::Light},  // by class
    {"MJ77", Icon::Light},  // by class
    {"MJ8", Icon::Light},  // by class
    {"MJ80", Icon::Light},  // by class
    {"MJ9", Icon::Light},  // by class
    {"MJ90", Icon::Light},  // by class
    {"MLER", Icon::Light},  // by class
    {"MM14", Icon::X_GYRO},
    {"MM16", Icon::X_GYRO},
    {"MM19", Icon::X_GYRO},
    {"MM21", Icon::X_GYRO},
    {"MM22", Icon::X_GYRO_CABIN},
    {"MM24", Icon::X_GYRO_CABIN},
    {"MMAC", Icon::Light},  // by class
    {"MMAX", Icon::X_GYRO},  // by class
    {"MMUT", Icon::Light},  // by class
    {"MNEX", Icon::Light},  // by class
    {"MOCU", Icon::Light},  // by class
    {"MOGO", Icon::Light},  // by class
    {"MOL1", Icon::Light},  // by class
    {"MON2", Icon::Light},  // by class
    {"MONA", Icon::Light},  // by class
    {"MONI", Icon::Light},  // by class
    {"MOR2", Icon::Light},  // by class
    {"MOSP", Icon::Light},  // by class
    {"MOSQ", Icon::X_MOSQ},
    {"MOTO", Icon::Light},  // by class
    {"MP02", Icon::Light},  // by class
    {"MP20", Icon::Light},  // by class
    {"MQ1", Icon::X_MQ9},
    {"MQ9", Icon::X_MQ9},
    {"MR25", Icon::Light},  // by class
    {"MR35", Icon::Light},  // by class
    {"MR3T", Icon::TurbopropSingle},  // by class
    {"MRAI", Icon::Light},  // by class
    {"MRAM", Icon::Light},  // by class
    {"MRF1", Icon::X_MRF1},
    {"MRJ7", Icon::BigBizJet},  // by class
    {"MRJ9", Icon::BigBizJet},  // by class
    {"MRMD", Icon::X_LA4},  // by class
    {"MRTN", Icon::Light},  // by class
    {"MS1", Icon::Light},  // by class
    {"MS18", Icon::Light},  // by class
    {"MS23", Icon::Light},  // by class
    {"MS25", Icon::Light},  // by class
    {"MS30", Icon::Light},  // by class
    {"MS31", Icon::Light},  // by class
    {"MS73", Icon::Light},  // by class
    {"MS76", Icon::BizJet},  // by class
    {"MSAI", Icon::Light},  // by class
    {"MSQ2", Icon::Light},  // by class
    {"MT", Icon::X_GYRO},
    {"MT2", Icon::BigBizJet},  // by class
    {"MT7", Icon::X_M7},
    {"MU2", Icon::X_MU2},
    {"MU23", Icon::Light},  // by class
    {"MU30", Icon::BigBizJet},  // by class
    {"MUS2", Icon::X_RV7},
    {"MVN1", Icon::Light},  // by class
    {"MVRK", Icon::Light},  // by class
    {"MX10", Icon::Light},  // by class
    {"MX1T", Icon::Light},  // by class
    {"MX2", Icon::Light},  // by class
    {"MX58", Icon::Light},  // by class
    {"MX65", Icon::Light},  // by class
    {"MX80", Icon::Light},  // by class
    {"MXS", Icon::X_E300},
    {"MY12", Icon::Light},  // by class
    {"MY13", Icon::Light},  // by class
    {"MYA4", Icon::Freighter},  // by class
    {"MYS4", Icon::JetTrainer},  // by class
    {"N110", Icon::Light},  // by class
    {"N120", Icon::Light},  // by class
    {"N219", Icon::Turboprop},  // by class
    {"N250", Icon::Turboprop},  // by class
    {"N260", Icon::Turboprop},  // by class
    {"N262", Icon::Turboprop},  // by class
    {"N3", Icon::Light},  // by class
    {"N320", Icon::Light},  // by class
    {"N340", Icon::Light},  // by class
    {"N3N", Icon::X_STEARMAN},
    {"N5", Icon::Light},  // by class
    {"N5A", Icon::Light},  // by class
    {"N5B", Icon::TurbopropSingle},  // by class
    {"NA40", Icon::Heli4},  // by class
    {"NAL2", Icon::Light},  // by class
    {"NARN", Icon::Light},  // by class
    {"NAVI", Icon::X_PA24},
    {"NC85", Icon::Light},  // by class
    {"ND1T", Icon::TurbopropSingle},  // by class
    {"NDAC", Icon::Light},  // by class
    {"NDAT", Icon::Light},  // by class
    {"NDIC", Icon::Light},  // by class
    {"NFEX", Icon::TurbopropSingle},  // by class
    {"NG4", Icon::Light},  // by class
    {"NG5", Icon::X_RV12},
    {"NG5E", Icon::Light},  // by class
    {"NH90", Icon::X_NH90},
    {"NHCO", Icon::Light},  // by class
    {"NI28", Icon::Light},  // by class
    {"NIBB", Icon::Light},  // by class
    {"NIMB", Icon::X_GLIDER_OPEN},
    {"NIPR", Icon::Light},  // by class
    {"NM5", Icon::Light},  // by class
    {"NMCU", Icon::Light},  // by class
    {"NNJA", Icon::Light},  // by class
    {"NOMA", Icon::TwinTurboprop},  // by class
    {"NORA", Icon::TwinPiston},  // by class
    {"NORS", Icon::Light},  // by class
    {"NPOR", Icon::X_CAMEL},
    {"NST6", Icon::Light},  // by class
    {"NSTR", Icon::Light},  // by class
    {"NT10", Icon::Light},  // by class
    {"NXT", Icon::Light},  // by class
    {"NXTE", Icon::Light},  // by class
    {"O1", Icon::X_C180},
    {"O2", Icon::X_C337},
    {"O3", Icon::Light},  // by class
    {"OCNR", Icon::Light},  // by class
    {"OH1", Icon::Heli4},  // by class
    {"OKHO", Icon::X_GYRO},  // by class
    {"OM1", Icon::Light},  // by class
    {"OMAG", Icon::Light},  // by class
    {"OMGA", Icon::Light},  // by class
    {"OMLA", Icon::TurbopropSingle},  // by class
    {"OMSD", Icon::X_LA4},  // by class
    {"ONE", Icon::Light},  // by class
    {"ONEX", Icon::Light},  // by class
    {"OPCA", Icon::Light},  // by class
    {"OR10", Icon::X_LA4},  // by class
    {"OR12", Icon::X_G21},  // by class
    {"OSCR", Icon::Light},  // by class
    {"OUDE", Icon::Light},  // by class
    {"OVOD", Icon::Light},  // by class
    {"OZZI", Icon::Light},  // by class
    {"P06T", Icon::X_P06T},
    {"P1", Icon::X_BAE146},  // by class
    {"P100", Icon::Light},  // by class
    {"P130", Icon::Light},  // by class
    {"P136", Icon::X_G21},  // by class
    {"P148", Icon::Light},  // by class
    {"P149", Icon::Light},  // by class
    {"P180", Icon::X_P180},
    {"P18T", Icon::X_PA18},
    {"P19", Icon::Light},  // by class
    {"P1HH", Icon::TwinTurboprop},  // by class
    {"P2", Icon::TwinPiston},  // by class
    {"P208", Icon::X_KITFOX},
    {"P210", Icon::X_C210},
    {"P212", Icon::X_C408},
    {"P220", Icon::Light},  // by class
    {"P230", Icon::Light},  // by class
    {"P250", Icon::Light},  // by class
    {"P25B", Icon::Light},  // by class
    {"P27", Icon::Light},  // by class
    {"P270", Icon::Light},  // by class
    {"P28A", Icon::X_PA28},
    {"P28B", Icon::X_PA28},
    {"P28R", Icon::X_PA28},
    {"P28S", Icon::X_PA28},
    {"P28T", Icon::X_PA28},
    {"P28U", Icon::X_PA28},
    {"P3", Icon::X_P3},
    {"P32R", Icon::X_PA32},
    {"P32T", Icon::X_PA32},
    {"P337", Icon::X_C337},
    {"P38", Icon::X_P38},
    {"P39", Icon::X_P39},
    {"P40", Icon::X_P40},
    {"P46T", Icon::X_PA46},
    {"P47", Icon::X_P47},
    {"P4Y", Icon::X_DC6},  // by class
    {"P50", Icon::Light},  // by class
    {"P51", Icon::X_P51},
    {"P57", Icon::Light},  // by class
    {"P60", Icon::Light},  // by class
    {"P61", Icon::TwinPiston},  // by class
    {"P63", Icon::X_P39},
    {"P66P", Icon::TwinPiston},  // by class
    {"P66T", Icon::TwinTurboprop},  // by class
    {"P68", Icon::X_P68},
    {"P68T", Icon::TwinTurboprop},  // by class
    {"P70", Icon::Light},  // by class
    {"P750", Icon::X_P750},
    {"P8", Icon::X_B738},
    {"P80", Icon::X_T33},
    {"P82", Icon::X_P82},
    {"P92", Icon::X_KITFOX},
    {"PA11", Icon::X_J3},
    {"PA12", Icon::X_PA18},
    {"PA14", Icon::X_PA18},
    {"PA15", Icon::X_PA18},
    {"PA16", Icon::X_PA18},
    {"PA17", Icon::X_PA18},
    {"PA18", Icon::X_PA18},
    {"PA20", Icon::X_PA18},
    {"PA22", Icon::X_PA18},
    {"PA23", Icon::X_PA23},
    {"PA24", Icon::X_PA24},
    {"PA25", Icon::X_PA25},
    {"PA27", Icon::TwinPiston},
    {"PA28", Icon::X_PA28},
    {"PA30", Icon::TwinPiston},
    {"PA31", Icon::X_PA31},
    {"PA32", Icon::X_PA32},
    {"PA34", Icon::X_PA34},
    {"PA36", Icon::X_PA25},
    {"PA38", Icon::X_PA38},
    {"PA3T", Icon::X_PA31},
    {"PA42", Icon::TwinTurboprop},
    {"PA44", Icon::X_PA44},
    {"PA46", Icon::X_PA46},
    {"PA47", Icon::JetTrainer},  // by class
    {"PACE", Icon::X_PA24},
    {"PAGO", Icon::Light},  // by class
    {"PANT", Icon::Light},  // by class
    {"PAR1", Icon::Light},  // by class
    {"PAR4", Icon::Light},  // by class
    {"PARA", Icon::X_PPC},
    {"PARL", Icon::X_J3},
    {"PAT2", Icon::Light},  // by class
    {"PAT4", Icon::TwinTurboprop},  // by class
    {"PAUL", Icon::Light},  // by class
    {"PAV4", Icon::X_GYRO},  // by class
    {"PAY1", Icon::X_PAY3},
    {"PAY2", Icon::X_PAY3},
    {"PAY3", Icon::X_PAY3},
    {"PAY4", Icon::X_PAY3},
    {"PBY", Icon::X_PBY},
    {"PC12", Icon::X_PC12},
    {"PC21", Icon::TurbopropSingle},
    {"PC24", Icon::X_PC24},
    {"PC6P", Icon::X_PC6},
    {"PC6T", Icon::X_PC6},
    {"PC7", Icon::TurbopropSingle},
    {"PC9", Icon::TurbopropSingle},
    {"PCA2", Icon::X_GYRO},  // by class
    {"PDIG", Icon::TwinPiston},  // by class
    {"PECR", Icon::Light},  // by class
    {"PEGA", Icon::TwinPiston},  // by class
    {"PEGZ", Icon::Light},  // by class
    {"PELI", Icon::Light},  // by class
    {"PEMB", Icon::TwinPiston},  // by class
    {"PETL", Icon::Light},  // by class
    {"PETR", Icon::X_PETREL},
    {"PGEE", Icon::Light},  // by class
    {"PGK1", Icon::Light},  // by class
    {"PHIL", Icon::Heli2},  // by class
    {"PHIX", Icon::X_GYRO},  // by class
    {"PHNX", Icon::Light},  // by class
    {"PIAE", Icon::Light},  // by class
    {"PIAT", Icon::Ultralight},
    {"PICO", Icon::Light},  // by class
    {"PILL", Icon::Light},  // by class
    {"PINO", Icon::Light},  // by class
    {"PIPA", Icon::X_SR22},
    {"PISI", Icon::X_MOTORGLIDER},
    {"PIT4", Icon::X_MOTORGLIDER},
    {"PIVE", Icon::Light},  // by class
    {"PIVI", Icon::X_MOTORGLIDER},
    {"PK11", Icon::Light},  // by class
    {"PK15", Icon::Light},  // by class
    {"PK18", Icon::Light},  // by class
    {"PK19", Icon::Light},  // by class
    {"PK21", Icon::Light},  // by class
    {"PK23", Icon::Light},  // by class
    {"PK25", Icon::Light},  // by class
    {"PKAN", Icon::Light},  // by class
    {"PL1", Icon::Light},  // by class
    {"PL12", Icon::Light},  // by class
    {"PL2", Icon::Light},  // by class
    {"PL4", Icon::Light},  // by class
    {"PL9", Icon::Light},  // by class
    {"PLUS", Icon::Light},  // by class
    {"PNR2", Icon::Light},  // by class
    {"PNR3", Icon::X_RV12},
    {"PNR4", Icon::Light},  // by class
    {"PNTH", Icon::Light},  // by class
    {"PO2", Icon::Light},  // by class
    {"PO60", Icon::Light},  // by class
    {"POLI", Icon::Light},  // by class
    {"PONY", Icon::X_LA4},  // by class
    {"PP2", Icon::Light},  // by class
    {"PP3", Icon::Light},  // by class
    {"PPRO", Icon::Light},  // by class
    {"PRBP", Icon::Light},  // by class
    {"PRBR", Icon::Light},  // by class
    {"PRCE", Icon::TwinPiston},  // by class
    {"PREN", Icon::Light},  // by class
    {"PRET", Icon::Light},  // by class
    {"PREX", Icon::Light},  // by class
    {"PRIM", Icon::Light},  // by class
    {"PRM1", Icon::X_PRM1},
    {"PROC", Icon::Light},  // by class
    {"PROT", Icon::Light},  // by class
    {"PROW", Icon::Light},  // by class
    {"PRPR", Icon::Light},  // by class
    {"PRTS", Icon::BizJet},  // by class
    {"PRXT", Icon::TurbopropSingle},  // by class
    {"PSTM", Icon::X_SPORTBIP},
    {"PSW4", Icon::Heli3},  // by class
    {"PT21", Icon::Light},  // by class
    {"PT22", Icon::X_DHC1},
    {"PT70", Icon::Light},  // by class
    {"PT80", Icon::Light},  // by class
    {"PTMS", Icon::Light},  // by class
    {"PTRL", Icon::Light},  // by class
    {"PTS1", Icon::X_PITTS},
    {"PTS2", Icon::X_PITTS},
    {"PTSS", Icon::Light},  // by class
    {"PUL6", Icon::Light},  // by class
    {"PULR", Icon::Light},  // by class
    {"PULS", Icon::X_RV12},
    {"PUMA", Icon::X_EC25},
    {"PUP", Icon::Light},  // by class
    {"PURS", Icon::Light},  // by class
    {"PUSH", Icon::Light},  // by class
    {"PVTR", Icon::X_LA4},  // by class
    {"PW4", Icon::Light},  // by class
    {"PW5", Icon::X_GLIDER},
    {"PZ01", Icon::Light},  // by class
    {"PZ02", Icon::Light},  // by class
    {"PZ04", Icon::X_WILGA},
    {"PZ05", Icon::Light},  // by class
    {"PZ06", Icon::Light},  // by class
    {"PZ12", Icon::Light},  // by class
    {"PZ26", Icon::Light},  // by class
    {"PZ3T", Icon::TurbopropSingle},  // by class
    {"PZ4M", Icon::Light},  // by class
    {"PZ6T", Icon::TurbopropSingle},  // by class
    {"Q01", Icon::Light},  // by class
    {"Q1", Icon::X_MQ9},
    {"Q25", Icon::JetTrainer},  // by class
    {"Q28", Icon::JetTrainer},  // by class
    {"Q4", Icon::X_Q4},
    {"Q5", Icon::BigBizJet},  // by class
    {"Q58", Icon::JetTrainer},  // by class
    {"Q9", Icon::X_MQ9},
    {"QAIL", Icon::Light},  // by class
    {"QALT", Icon::Light},  // by class
    {"QEST", Icon::Light},  // by class
    {"QIC2", Icon::X_DFLY},
    {"QR01", Icon::Light},  // by class
    {"QUAS", Icon::Light},  // by class
    {"QUIC", Icon::X_DFLY},
    {"QUIN", Icon::X_GLIDER_OPEN},
    {"R100", Icon::Light},  // by class
    {"R109", Icon::Light},  // by class
    {"R11", Icon::Light},  // by class
    {"R12", Icon::Light},  // by class
    {"R135", Icon::X_KC135},
    {"R185", Icon::Light},  // by class
    {"R2", Icon::Light},  // by class
    {"R200", Icon::Light},  // by class
    {"R22", Icon::X_R22},
    {"R300", Icon::Light},  // by class
    {"R4", Icon::Heli2},  // by class
    {"R44", Icon::X_R44},
    {"R66", Icon::X_R66},
    {"R721", Icon::Falcon},  // by class
    {"R722", Icon::Falcon},  // by class
    {"R90F", Icon::Light},  // by class
    {"R90R", Icon::Light},  // by class
    {"R90T", Icon::TurbopropSingle},  // by class
    {"RA14", Icon::Light},  // by class
    {"RA17", Icon::Light},  // by class
    {"RAF2", Icon::X_GYRO},
    {"RAID", Icon::Light},  // by class
    {"RAIL", Icon::Light},  // by class
    {"RALL", Icon::X_TB20},
    {"RANG", Icon::Light},  // by class
    {"RARO", Icon::Light},  // by class
    {"RAV3", Icon::Light},  // by class
    {"RAV5", Icon::Light},  // by class
    {"RAZM", Icon::Light},  // by class
    {"RBEL", Icon::X_KITFOX},
    {"RC3", Icon::X_LA4},
    {"RC70", Icon::TwinPiston},  // by class
    {"RCAL", Icon::Light},  // by class
    {"RD03", Icon::Light},  // by class
    {"RD20", Icon::Light},  // by class
    {"RDH2", Icon::Light},  // by class
    {"RELI", Icon::X_C180},
    {"RENE", Icon::Light},  // by class
    {"REV6", Icon::X_GYRO},  // by class
    {"RF10", Icon::Light},  // by class
    {"RF3", Icon::Light},  // by class
    {"RF4", Icon::Light},  // by class
    {"RF47", Icon::Light},  // by class
    {"RF5", Icon::Light},  // by class
    {"RF6", Icon::Light},  // by class
    {"RF9", Icon::Light},  // by class
    {"RFAL", Icon::X_RAFALE},
    {"RGNT", Icon::Light},  // by class
    {"RISN", Icon::Light},  // by class
    {"RJ03", Icon::Light},  // by class
    {"RJ1H", Icon::X_BAE146_300},
    {"RJ70", Icon::X_BAE146},
    {"RJ85", Icon::X_BAE146},
    {"RK5", Icon::Light},  // by class
    {"RLU1", Icon::Light},  // by class
    {"RMOU", Icon::Heli3},  // by class
    {"RNGR", Icon::Light},  // by class
    {"RODS", Icon::X_E300},
    {"ROND", Icon::Light},  // by class
    {"ROSE", Icon::Light},  // by class
    {"RP1", Icon::Heli4},  // by class
    {"RPUP", Icon::X_GYRO},  // by class
    {"RQ4", Icon::X_Q4},
    {"RS12", Icon::X_RS12},
    {"RS14", Icon::X_RS12},
    {"RS18", Icon::Light},  // by class
    {"RS20", Icon::X_KITFOX},
    {"RS21", Icon::X_KITFOX},
    {"RTA4", Icon::Light},  // by class
    {"RUBI", Icon::Light},  // by class
    {"RV10", Icon::X_RV10},
    {"RV12", Icon::X_RV12},
    {"RV14", Icon::X_RV7},
    {"RV15", Icon::Light},  // by class
    {"RV3", Icon::X_RV7},
    {"RV4", Icon::X_RV7},
    {"RV4T", Icon::TurbopropSingle},  // by class
    {"RV6", Icon::X_RV7},
    {"RV7", Icon::X_RV7},
    {"RV8", Icon::X_RV7},
    {"RV9", Icon::X_RV7},
    {"RVAL", Icon::Heli4},  // by class
    {"RVST", Icon::Light},  // by class
    {"RW19", Icon::Light},  // by class
    {"RW20", Icon::Light},  // by class
    {"RW22", Icon::Light},  // by class
    {"RW26", Icon::Light},  // by class
    {"RW3", Icon::Light},  // by class
    {"RYSA", Icon::TwinTurboprop},  // by class
    {"RYST", Icon::Light},  // by class
    {"S05F", Icon::Light},  // by class
    {"S05R", Icon::Light},  // by class
    {"S1", Icon::X_J3},
    {"S10", Icon::X_C180},
    {"S107", Icon::Light},  // by class
    {"S108", Icon::X_C180},
    {"S11", Icon::Light},  // by class
    {"S12", Icon::X_LA4},  // by class
    {"S122", Icon::TwinPiston},  // by class
    {"S15S", Icon::X_MOTORGLIDER},
    {"S15U", Icon::Light},  // by class
    {"S160", Icon::Light},  // by class
    {"S200", Icon::JetTrainer},  // by class
    {"S202", Icon::X_G21},  // by class
    {"S208", Icon::Light},  // by class
    {"S21", Icon::Light},  // by class
    {"S211", Icon::JetTrainer},  // by class
    {"S223", Icon::Light},  // by class
    {"S22T", Icon::X_SR22},
    {"S274", Icon::Heli3},  // by class
    {"S278", Icon::Heli3},  // by class
    {"S285", Icon::Heli3},  // by class
    {"S2P", Icon::X_S2},
    {"S2T", Icon::X_S2},
    {"S3", Icon::BigBizJet},  // by class
    {"S300", Icon::X_H269},
    {"S330", Icon::X_H269},
    {"S355", Icon::Light},  // by class
    {"S360", Icon::Heli3},  // by class
    {"S37", Icon::BigBizJet},  // by class
    {"S38", Icon::X_G21},  // by class
    {"S39", Icon::X_LA4},  // by class
    {"S4", Icon::Light},  // by class
    {"S400", Icon::TwinPiston},  // by class
    {"S401", Icon::JetTrainer},  // by class
    {"S434", Icon::Heli3},  // by class
    {"S45", Icon::Light},  // by class
    {"S450", Icon::Light},  // by class
    {"S51", Icon::Heli2},  // by class
    {"S51D", Icon::Light},  // by class
    {"S52", Icon::Heli2},  // by class
    {"S522", Icon::Light},  // by class
    {"S55P", Icon::Heli2},  // by class
    {"S55T", Icon::Heli3},  // by class
    {"S58P", Icon::Heli2},  // by class
    {"S58T", Icon::Heli3},  // by class
    {"S6", Icon::X_MOTORGLIDER},
    {"S601", Icon::BizJet},  // by class
    {"S61", Icon::X_S61},
    {"S61R", Icon::Heli4},  // by class
    {"S62", Icon::Heli3},  // by class
    {"S64", Icon::Heli4},  // by class
    {"S65C", Icon::Heli4},  // by class
    {"S70", Icon::X_H60},
    {"S716", Icon::JetTrainer},  // by class
    {"S76", Icon::X_S76},
    {"S900", Icon::Light},  // by class
    {"S92", Icon::X_S92},
    {"S97", Icon::Heli3},  // by class
    {"SA02", Icon::Light},  // by class
    {"SA03", Icon::Light},  // by class
    {"SA04", Icon::Light},  // by class
    {"SA05", Icon::Light},  // by class
    {"SA10", Icon::X_SPORTBIP},
    {"SA11", Icon::Light},  // by class
    {"SA2", Icon::Light},  // by class
    {"SA20", Icon::X_LA4},  // by class
    {"SA3", Icon::Light},  // by class
    {"SA30", Icon::X_SPORTBIP},
    {"SA37", Icon::X_MOTORGLIDER},
    {"SA38", Icon::TwinPiston},  // by class
    {"SA50", Icon::Light},  // by class
    {"SA6", Icon::Light},  // by class
    {"SA6E", Icon::TurbopropSingle},  // by class
    {"SA7", Icon::Light},  // by class
    {"SA70", Icon::Light},  // by class
    {"SA75", Icon::X_SPORTBIP},
    {"SA8T", Icon::TwinTurboprop},  // by class
    {"SAB2", Icon::Light},  // by class
    {"SABA", Icon::Light},  // by class
    {"SABW", Icon::Light},  // by class
    {"SACE", Icon::Light},  // by class
    {"SACR", Icon::Light},  // by class
    {"SAFF", Icon::Light},  // by class
    {"SAH1", Icon::Light},  // by class
    {"SAKO", Icon::Light},  // by class
    {"SALB", Icon::Light},  // by class
    {"SAM", Icon::Light},  // by class
    {"SAND", Icon::X_DC6},  // by class
    {"SAPH", Icon::Light},  // by class
    {"SASH", Icon::TwinPiston},  // by class
    {"SASP", Icon::Light},  // by class
    {"SASY", Icon::Light},  // by class
    {"SATA", Icon::BizJet},  // by class
    {"SAVA", Icon::Light},  // by class
    {"SAVG", Icon::X_KITFOX},
    {"SB05", Icon::BizJet},  // by class
    {"SB1", Icon::Heli4},  // by class
    {"SB20", Icon::X_SB20},
    {"SB29", Icon::JetTrainer},  // by class
    {"SB32", Icon::JetTrainer},  // by class
    {"SB35", Icon::X_DRAKEN},
    {"SB37", Icon::X_VIGGEN},
    {"SB39", Icon::X_GRIPEN},
    {"SB7", Icon::Light},  // by class
    {"SB91", Icon::Light},  // by class
    {"SBD", Icon::X_SBD},
    {"SBLS", Icon::TwinPiston},  // by class
    {"SBM3", Icon::Light},  // by class
    {"SBOY", Icon::Light},  // by class
    {"SBR1", Icon::X_SBR1},
    {"SBR2", Icon::X_SBR1},
    {"SC01", Icon::Light},  // by class
    {"SC7", Icon::X_SC7},
    {"SCAM", Icon::Light},  // by class
    {"SCEP", Icon::Light},  // by class
    {"SCII", Icon::X_GYRO},
    {"SCIR", Icon::X_GLIDER},
    {"SCOM", Icon::Light},  // by class
    {"SCOR", Icon::Heli2},  // by class
    {"SCOU", Icon::Heli3},  // by class
    {"SCRO", Icon::Light},  // by class
    {"SCTR", Icon::Light},  // by class
    {"SCUB", Icon::Light},  // by class
    {"SCW1", Icon::Light},  // by class
    {"SD2", Icon::Light},  // by class
    {"SD26", Icon::Light},  // by class
    {"SD4", Icon::X_RV12},
    {"SDUS", Icon::Light},  // by class
    {"SE1", Icon::Light},  // by class
    {"SE5A", Icon::Light},  // by class
    {"SE5R", Icon::Light},  // by class
    {"SEAT", Icon::X_LA4},  // by class
    {"SEAW", Icon::X_LA4},  // by class
    {"SERA", Icon::Light},  // by class
    {"SERV", Icon::Ground},
    {"SEST", Icon::X_LA4},  // by class
    {"SF2", Icon::Light},  // by class
    {"SF23", Icon::X_MOTORGLIDER},
    {"SF24", Icon::X_MOTORGLIDER},
    {"SF25", Icon::X_MOTORGLIDER},
    {"SF28", Icon::X_MOTORGLIDER},
    {"SF31", Icon::X_MOTORGLIDER},
    {"SF32", Icon::X_MOTORGLIDER},
    {"SF34", Icon::X_SF34},
    {"SF35", Icon::X_MOTORGLIDER},
    {"SF36", Icon::X_MOTORGLIDER},
    {"SF50", Icon::X_SF50},
    {"SG37", Icon::X_MOTORGLIDER},
    {"SG70", Icon::Light},  // by class
    {"SG92", Icon::TurbopropSingle},  // by class
    {"SGCD", Icon::X_GRIPEN},
    {"SGEF", Icon::X_GRIPEN},
    {"SGRA", Icon::Light},  // by class
    {"SGUP", Icon::X_SGUP},
    {"SH09", Icon::Heli3},  // by class
    {"SH33", Icon::X_SC7},
    {"SH36", Icon::X_SC7},
    {"SH4", Icon::Heli2},  // by class
    {"SH5", Icon::C130},  // by class
    {"SHAC", Icon::X_DC6},  // by class
    {"SHAK", Icon::X_LA4},  // by class
    {"SHAW", Icon::JetTrainer},  // by class
    {"SHEA", Icon::X_LA4},  // by class
    {"SHEK", Icon::Light},  // by class
    {"SHER", Icon::Light},  // by class
    {"SHIP", Icon::X_BLIMP},
    {"SHOE", Icon::Light},  // by class
    {"SHOP", Icon::Light},  // by class
    {"SHOR", Icon::Light},  // by class
    {"SHRA", Icon::Light},  // by class
    {"SHRK", Icon::Light},  // by class
    {"SHRT", Icon::TurbopropSingle},  // by class
    {"SIDE", Icon::Light},  // by class
    {"SIGM", Icon::Light},  // by class
    {"SILH", Icon::Light},  // by class
    {"SIR2", Icon::Light},  // by class
    {"SIRA", Icon::X_RV12},
    {"SJ30", Icon::BizJet},  // by class
    {"SJET", Icon::JetTrainer},  // by class
    {"SK10", Icon::Light},  // by class
    {"SK70", Icon::TwinPiston},  // by class
    {"SKAR", Icon::Light},  // by class
    {"SKIF", Icon::X_LA4},  // by class
    {"SKIM", Icon::X_LA4},  // by class
    {"SKRA", Icon::X_KITFOX},
    {"SKYC", Icon::TwinPiston},  // by class
    {"SKYO", Icon::Light},  // by class
    {"SKYR", Icon::Light},  // by class
    {"SL1", Icon::Light},  // by class
    {"SL39", Icon::Light},  // by class
    {"SL90", Icon::Light},  // by class
    {"SLCH", Icon::X_SLCH},
    {"SLG2", Icon::X_SLING},
    {"SLG4", Icon::X_SLING},
    {"SLGT", Icon::X_SLING},
    {"SLH4", Icon::X_KITFOX},
    {"SLK3", Icon::Light},  // by class
    {"SLK5", Icon::Light},  // by class
    {"SM01", Icon::Light},  // by class
    {"SM19", Icon::TurbopropSingle},  // by class
    {"SM20", Icon::TurbopropSingle},  // by class
    {"SM60", Icon::X_TRIMOTOR},
    {"SM92", Icon::Light},  // by class
    {"SMAX", Icon::X_LA4},  // by class
    {"SMB2", Icon::JetTrainer},  // by class
    {"SNAD", Icon::X_GYRO},  // by class
    {"SNAP", Icon::X_RV12},
    {"SNGY", Icon::X_LNC4},
    {"SNOS", Icon::Light},  // by class
    {"SNS2", Icon::Light},  // by class
    {"SNS7", Icon::Light},  // by class
    {"SNS9", Icon::Light},  // by class
    {"SOK2", Icon::Light},  // by class
    {"SOKL", Icon::Light},  // by class
    {"SOL1", Icon::X_DC6},  // by class
    {"SOL2", Icon::X_DC6},  // by class
    {"SONX", Icon::X_RV12},
    {"SORA", Icon::Light},  // by class
    {"SP20", Icon::Light},  // by class
    {"SP33", Icon::BizJet},  // by class
    {"SP55", Icon::Light},  // by class
    {"SP6E", Icon::Light},  // by class
    {"SP7", Icon::X_PA24},
    {"SP91", Icon::Light},  // by class
    {"SP95", Icon::Light},  // by class
    {"SPA2", Icon::Light},  // by class
    {"SPAR", Icon::Light},  // by class
    {"SPC2", Icon::Light},  // by class
    {"SPDR", Icon::Light},  // by class
    {"SPEL", Icon::Light},  // by class
    {"SPGY", Icon::X_GYRO},
    {"SPHA", Icon::X_GYRO},
    {"SPIR", Icon::Light},  // by class
    {"SPIT", Icon::X_SPIT},
    {"SPKR", Icon::Light},  // by class
    {"SPOR", Icon::Light},  // by class
    {"SPR2", Icon::Light},  // by class
    {"SPRT", Icon::Light},  // by class
    {"SPST", Icon::Light},  // by class
    {"SPUP", Icon::X_CAMEL},
    {"SQ2T", Icon::X_DFLY},
    {"SQES", Icon::TwinPiston},  // by class
    {"SR01", Icon::Light},  // by class
    {"SR20", Icon::X_SR22},
    {"SR22", Icon::X_SR22},
    {"SR71", Icon::X_SR71},
    {"SRAC", Icon::Light},  // by class
    {"SRAI", Icon::X_RV12},
    {"SRAS", Icon::Light},  // by class
    {"SRAY", Icon::X_LA4},  // by class
    {"SREY", Icon::X_ICONA5},
    {"SS2", Icon::X_SS2},
    {"SS2P", Icon::X_AT8T},
    {"SS2T", Icon::X_AT8T},
    {"SSAB", Icon::JetTrainer},  // by class
    {"SSC", Icon::X_GYRO},  // by class
    {"SSTL", Icon::X_KITFOX},
    {"SSTM", Icon::X_LA4},  // by class
    {"ST1", Icon::TwinPiston},  // by class
    {"ST10", Icon::Light},  // by class
    {"ST3", Icon::X_SPORTBIP},
    {"ST30", Icon::X_E300},
    {"ST4", Icon::Light},  // by class
    {"ST50", Icon::TurbopropSingle},  // by class
    {"ST6", Icon::X_STEARMAN},
    {"ST60", Icon::Light},  // by class
    {"ST75", Icon::X_STEARMAN},
    {"ST87", Icon::Light},  // by class
    {"STAL", Icon::Light},  // by class
    {"STAR", Icon::X_STARSHIP},
    {"STAT", Icon::TurbopropSingle},  // by class
    {"STCH", Icon::X_STORCH},
    {"STFF", Icon::Light},  // by class
    {"STG2", Icon::Light},  // by class
    {"STIL", Icon::Light},  // by class
    {"STLN", Icon::TurbopropSingle},  // by class
    {"STOR", Icon::X_STORCH},
    {"STRA", Icon::Light},  // by class
    {"STRE", Icon::Light},  // by class
    {"STRI", Icon::Light},  // by class
    {"STRK", Icon::X_JPRO},
    {"STRM", Icon::Light},  // by class
    {"STST", Icon::Light},  // by class
    {"SU17", Icon::JetTrainer},  // by class
    {"SU24", Icon::X_F111},
    {"SU25", Icon::X_SU25},
    {"SU26", Icon::X_SU26},
    {"SU27", Icon::X_SU27},
    {"SU29", Icon::X_SU26},
    {"SU30", Icon::X_SU27},
    {"SU31", Icon::X_SU26},
    {"SU33", Icon::X_SU27},
    {"SU34", Icon::FighterTwin},
    {"SU35", Icon::X_SU27},
    {"SU38", Icon::Light},  // by class
    {"SU57", Icon::FighterTwin},
    {"SU7", Icon::JetTrainer},  // by class
    {"SU80", Icon::Turboprop},  // by class
    {"SU95", Icon::X_SU95},
    {"SUBA", Icon::Light},  // by class
    {"SUCO", Icon::X_AH1},
    {"SUNB", Icon::Light},  // by class
    {"SUNV", Icon::Light},  // by class
    {"SURN", Icon::Heli4},  // by class
    {"SURU", Icon::Light},  // by class
    {"SUSO", Icon::Light},  // by class
    {"SV4", Icon::X_SPORTBIP},
    {"SVNH", Icon::X_KITFOX},
    {"SW18", Icon::Light},  // by class
    {"SW2", Icon::X_SW4},
    {"SW3", Icon::X_SW4},
    {"SW4", Icon::X_SW4},
    {"SWAK", Icon::Light},  // by class
    {"SWAT", Icon::Light},  // by class
    {"SWFT", Icon::JetTrainer},  // by class
    {"SWIF", Icon::JetTrainer},  // by class
    {"SWIN", Icon::Light},  // by class
    {"SWOR", Icon::Light},  // by class
    {"SX30", Icon::Light},  // by class
    {"SYCA", Icon::Heli2},  // by class
    {"SYMP", Icon::Light},  // by class
    {"SYNC", Icon::Light},  // by class
    {"SZ45", Icon::Light},  // by class
    {"SZ9M", Icon::Light},  // by class
    {"T1", Icon::JetTrainer},  // by class
    {"T10", Icon::Light},  // by class
    {"T101", Icon::TurbopropSingle},  // by class
    {"T134", Icon::X_T134},
    {"T142", Icon::X_TU95},
    {"T154", Icon::X_T154},
    {"T160", Icon::B1},
    {"T18", Icon::X_RV7},
    {"T19", Icon::Light},  // by class
    {"T2", Icon::BizJet},  // by class
    {"T204", Icon::X_B752},
    {"T206", Icon::X_C182},
    {"T210", Icon::X_C210},
    {"T211", Icon::Light},  // by class
    {"T22M", Icon::BigBizJet},  // by class
    {"T250", Icon::X_PA24},
    {"T28", Icon::X_T28},
    {"T30", Icon::Light},  // by class
    {"T33", Icon::X_T33},
    {"T334", Icon::BigBizJet},  // by class
    {"T34", Icon::X_T34},
    {"T34P", Icon::X_T34},
    {"T34T", Icon::X_T34},
    {"T35", Icon::Light},  // by class
    {"T37", Icon::X_A37},
    {"T38", Icon::X_F5},
    {"T4", Icon::BigBizJet},  // by class
    {"T40", Icon::Light},  // by class
    {"T411", Icon::Light},  // by class
    {"T415", Icon::Light},  // by class
    {"T419", Icon::Light},  // by class
    {"T45", Icon::X_HAWK},
    {"T5", Icon::TurbopropSingle},  // by class
    {"T50", Icon::JetTrainer},
    {"T51", Icon::Light},  // by class
    {"T5YY", Icon::BigBizJet},  // by class
    {"T6", Icon::X_T6},
    {"T7", Icon::X_T7},
    {"TA15", Icon::X_J3},
    {"TA16", Icon::X_LA4},
    {"TA20", Icon::Light},  // by class
    {"TAA1", Icon::Light},  // by class
    {"TAGO", Icon::Light},  // by class
    {"TAIL", Icon::X_KITFOX},
    {"TAMP", Icon::X_TB20},
    {"TARO", Icon::Light},  // by class
    {"TARR", Icon::Light},  // by class
    {"TAYA", Icon::Light},  // by class
    {"TAYB", Icon::X_J3},
    {"TAYD", Icon::X_J3},
    {"TB05", Icon::Light},  // by class
    {"TB10", Icon::X_TB20},
    {"TB20", Icon::X_TB20},
    {"TB21", Icon::X_TB20},
    {"TB30", Icon::X_T34},
    {"TB31", Icon::TurbopropSingle},  // by class
    {"TB9", Icon::X_TB20},
    {"TBEE", Icon::X_G21},  // by class
    {"TBM", Icon::X_TBMAV},
    {"TBM7", Icon::X_TBM9},
    {"TBM8", Icon::X_TBM9},
    {"TBM9", Icon::X_TBM9},
    {"TBR3", Icon::Light},  // by class
    {"TC2", Icon::Light},  // by class
    {"TCAT", Icon::TwinPiston},  // by class
    {"TCOU", Icon::TwinPiston},  // by class
    {"TD1", Icon::Light},  // by class
    {"TD2", Icon::TurbopropSingle},  // by class
    {"TD3", Icon::Light},  // by class
    {"TEAL", Icon::X_LA4},  // by class
    {"TERM", Icon::Light},  // by class
    {"TERR", Icon::Light},  // by class
    {"TEX2", Icon::TurbopropSingle},
    {"TEXA", Icon::Light},  // by class
    {"TF19", Icon::X_J3},
    {"TF21", Icon::X_J3},
    {"TF22", Icon::Light},  // by class
    {"TFOC", Icon::Light},  // by class
    {"TFUN", Icon::Light},  // by class
    {"TGRS", Icon::X_LNC4},
    {"TIAD", Icon::Light},  // by class
    {"TIGR", Icon::X_TIGR},
    {"TIJU", Icon::Light},  // by class
    {"TIPB", Icon::Light},  // by class
    {"TJET", Icon::BizJet},  // by class
    {"TL20", Icon::X_RV12},
    {"TL30", Icon::X_RV12},
    {"TLEG", Icon::TurbopropSingle},  // by class
    {"TM5", Icon::Light},  // by class
    {"TMOT", Icon::Light},  // by class
    {"TMUS", Icon::Light},  // by class
    {"TNAV", Icon::TwinPiston},  // by class
    {"TNDR", Icon::Light},  // by class
    {"TOBA", Icon::X_TB20},
    {"TOOT", Icon::Light},  // by class
    {"TOR", Icon::X_TORNADO},
    {"TOUR", Icon::Light},  // by class
    {"TOXO", Icon::Light},  // by class
    {"TP40", Icon::Light},  // by class
    {"TPIL", Icon::TurbopropSingle},  // by class
    {"TPIN", Icon::TwinPiston},  // by class
    {"TR1", Icon::X_LA4},  // by class
    {"TR20", Icon::Light},  // by class
    {"TR26", Icon::Light},  // by class
    {"TR55", Icon::Light},  // by class
    {"TRAL", Icon::Light},  // by class
    {"TRAP", Icon::Light},  // by class
    {"TRBA", Icon::Light},  // by class
    {"TRDO", Icon::Light},  // by class
    {"TRF1", Icon::Light},  // by class
    {"TRIM", Icon::X_TRIMOTOR},
    {"TRIS", Icon::X_TRIMOTOR},  // by class
    {"TRMA", Icon::X_TRIMOTOR},
    {"TRWN", Icon::TwinPiston},  // by class
    {"TS11", Icon::JetTrainer},  // by class
    {"TS1J", Icon::JetTrainer},  // by class
    {"TS8", Icon::Light},  // by class
    {"TSPT", Icon::Light},  // by class
    {"TSTN", Icon::Light},  // by class
    {"TSTR", Icon::X_GYRO_CABIN},
    {"TT62", Icon::TwinPiston},  // by class
    {"TTRS", Icon::Light},  // by class
    {"TTWO", Icon::Light},  // by class
    {"TU16", Icon::BigBizJet},  // by class
    {"TU22", Icon::BigBizJet},  // by class
    {"TU95", Icon::X_TU95},
    {"TUCA", Icon::TurbopropSingle},
    {"TUCR", Icon::X_T6},
    {"TUCT", Icon::TurbopropSingle},  // by class
    {"TUL3", Icon::Light},  // by class
    {"TUTR", Icon::Light},  // by class
    {"TVL4", Icon::Light},  // by class
    {"TVLB", Icon::Light},  // by class
    {"TWEN", Icon::X_RV12},
    {"TWIR", Icon::TwinPiston},  // by class
    {"TWR", Icon::Ground},
    {"TWSP", Icon::Light},  // by class
    {"TWST", Icon::Light},  // by class
    {"TZRV", Icon::JetTrainer},  // by class
    {"U15", Icon::Light},  // by class
    {"U16", Icon::X_G21},
    {"U2", Icon::X_U2},
    {"U21", Icon::X_BE20},
    {"U22", Icon::Light},  // by class
    {"UBAT", Icon::Light},  // by class
    {"UF10", Icon::Light},  // by class
    {"UF13", Icon::Light},  // by class
    {"UFHT", Icon::X_GYRO},  // by class
    {"UH1", Icon::X_UH1},
    {"UH12", Icon::X_B47G},
    {"UH1Y", Icon::X_UH1},
    {"UH60", Icon::X_H60},
    {"UHEL", Icon::Rotor},
    {"UL10", Icon::Light},  // by class
    {"UL20", Icon::Light},  // by class
    {"UL2F", Icon::Light},  // by class
    {"UL39", Icon::Light},  // by class
    {"UL45", Icon::Light},  // by class
    {"ULAC", Icon::Ultralight},
    {"ULPA", Icon::Light},  // by class
    {"ULTR", Icon::TwinPiston},  // by class
    {"ULTS", Icon::Heli2},  // by class
    {"UM18", Icon::X_GYRO},  // by class
    {"UNIV", Icon::Light},  // by class
    {"UPF7", Icon::X_WACO},
    {"URRA", Icon::Light},  // by class
    {"US1", Icon::Amphibian},  // by class
    {"US2", Icon::Amphibian},  // by class
    {"UT60", Icon::Light},  // by class
    {"UT65", Icon::Light},  // by class
    {"UT66", Icon::Light},  // by class
    {"UT75", Icon::Light},  // by class
    {"UU12", Icon::Light},  // by class
    {"V1", Icon::X_MOHAWK},
    {"V10", Icon::X_OV10},
    {"V22", Icon::Tiltrotor},
    {"V221", Icon::Light},  // by class
    {"V252", Icon::Light},  // by class
    {"V280", Icon::Tiltrotor},  // by class
    {"V322", Icon::Light},  // by class
    {"V351", Icon::Light},  // by class
    {"V452", Icon::TurbopropSingle},  // by class
    {"V500", Icon::Heli2},  // by class
    {"V8SP", Icon::Light},  // by class
    {"VALI", Icon::Light},  // by class
    {"VAMP", Icon::X_VAMP},
    {"VANT", Icon::JetTrainer},  // by class
    {"VAUT", Icon::BigBizJet},  // by class
    {"VELO", Icon::X_VELO},
    {"VELT", Icon::TwinPiston},  // by class
    {"VENT", Icon::X_GLIDER},
    {"VEZE", Icon::X_LGEZ},
    {"VF2", Icon::Light},  // by class
    {"VF35", Icon::X_F35},
    {"VF60", Icon::TurbopropSingle},  // by class
    {"VFHC", Icon::X_QUAD},
    {"VG3T", Icon::Light},  // by class
    {"VGUL", Icon::Light},  // by class
    {"VIGN", Icon::X_VIGGEN},
    {"VIMA", Icon::Light},  // by class
    {"VIMY", Icon::TwinPiston},  // by class
    {"VIPJ", Icon::JetTrainer},  // by class
    {"VIPR", Icon::Light},  // by class
    {"VISC", Icon::X_VISC},
    {"VISI", Icon::Light},  // by class
    {"VIX", Icon::X_KITFOX},
    {"VIXN", Icon::Light},  // by class
    {"VJ22", Icon::X_LA4},  // by class
    {"VK3P", Icon::Light},  // by class
    {"VK3T", Icon::TurbopropSingle},  // by class
    {"VL3", Icon::X_RV12},
    {"VL3T", Icon::TurbopropSingle},  // by class
    {"VLOT", Icon::TurbopropSingle},  // by class
    {"VLTT", Icon::TwinTurboprop},  // by class
    {"VM1", Icon::Light},  // by class
    {"VMT", Icon::Freighter},  // by class
    {"VNOM", Icon::JetTrainer},  // by class
    {"VNTR", Icon::Light},  // by class
    {"VO10", Icon::X_C172},
    {"VOL2", Icon::Light},  // by class
    {"VP2", Icon::Light},  // by class
    {"VR20", Icon::Light},  // by class
    {"VR7", Icon::X_RV12},
    {"VSON", Icon::Light},  // by class
    {"VTOR", Icon::TwinTurboprop},  // by class
    {"VTRA", Icon::Light},  // by class
    {"VTUR", Icon::Light},  // by class
    {"VUT1", Icon::Light},  // by class
    {"VVIG", Icon::X_LGEZ},
    {"VW10", Icon::Light},  // by class
    {"VWIT", Icon::Light},  // by class
    {"W11", Icon::Light},  // by class
    {"W135", Icon::Freighter},  // by class
    {"W201", Icon::Light},  // by class
    {"W3", Icon::Heli4},  // by class
    {"W5BC", Icon::Light},  // by class
    {"W62T", Icon::TurbopropSingle},  // by class
    {"WA40", Icon::Light},  // by class
    {"WA41", Icon::Light},  // by class
    {"WA42", Icon::Light},  // by class
    {"WA50", Icon::Light},  // by class
    {"WA80", Icon::Light},  // by class
    {"WAC9", Icon::Light},  // by class
    {"WACA", Icon::Light},  // by class
    {"WACC", Icon::X_WACO},
    {"WACD", Icon::Light},  // by class
    {"WACE", Icon::Light},  // by class
    {"WACF", Icon::X_WACO},
    {"WACG", Icon::Light},  // by class
    {"WACM", Icon::Light},  // by class
    {"WACN", Icon::Light},  // by class
    {"WACO", Icon::X_WACO},
    {"WACT", Icon::Light},  // by class
    {"WAIX", Icon::X_RV12},
    {"WASP", Icon::Heli3},  // by class
    {"WB57", Icon::BigBizJet},  // by class
    {"WBOO", Icon::Light},  // by class
    {"WCAT", Icon::X_HCAT},
    {"WDEX", Icon::Light},  // by class
    {"WESX", Icon::Heli4},  // by class
    {"WF4U", Icon::Light},  // by class
    {"WFOC", Icon::Light},  // by class
    {"WFUR", Icon::Light},  // by class
    {"WG30", Icon::Heli4},  // by class
    {"WH1", Icon::Light},  // by class
    {"WH4", Icon::TurbopropSingle},  // by class
    {"WHAT", Icon::Light},  // by class
    {"WHIL", Icon::Light},  // by class
    {"WHIS", Icon::Light},  // by class
    {"WHIT", Icon::Light},  // by class
    {"WHK2", Icon::X_WHK2},
    {"WHKN", Icon::X_WHK2},
    {"WICH", Icon::Light},  // by class
    {"WILT", Icon::Light},  // by class
    {"WIND", Icon::Light},  // by class
    {"WINE", Icon::Light},  // by class
    {"WIRR", Icon::Light},  // by class
    {"WISP", Icon::Light},  // by class
    {"WLBY", Icon::Light},  // by class
    {"WM2", Icon::Light},  // by class
    {"WOPU", Icon::Light},  // by class
    {"WP40", Icon::Light},  // by class
    {"WP47", Icon::Light},  // by class
    {"WS22", Icon::Light},  // by class
    {"WSP", Icon::Light},  // by class
    {"WT10", Icon::Light},  // by class
    {"WT9", Icon::X_RV12},
    {"WUSH", Icon::X_LA4},  // by class
    {"WW1", Icon::Light},  // by class
    {"WW23", Icon::X_WW24},
    {"WW24", Icon::X_WW24},
    {"WX35", Icon::Light},  // by class
    {"WZ10", Icon::HeliAttack},
    {"WZER", Icon::Light},  // by class
    {"X2", Icon::Heli3},  // by class
    {"X29", Icon::JetTrainer},  // by class
    {"X3", Icon::Heli4},  // by class
    {"X32", Icon::JetTrainer},  // by class
    {"X4", Icon::Light},  // by class
    {"X47B", Icon::JetTrainer},  // by class
    {"X49", Icon::Heli4},  // by class
    {"X55", Icon::BigBizJet},  // by class
    {"X59", Icon::X_X59},
    {"XA41", Icon::Light},  // by class
    {"XA42", Icon::Light},  // by class
    {"XA85", Icon::Light},  // by class
    {"XAIR", Icon::Light},  // by class
    {"XB1", Icon::Falcon},  // by class
    {"XCUB", Icon::X_XCUB},
    {"XL2", Icon::Light},  // by class
    {"XNON", Icon::X_GYRO_CABIN},
    {"XNOS", Icon::Light},  // by class
    {"XV15", Icon::Tiltrotor},  // by class
    {"Y11", Icon::TwinPiston},  // by class
    {"Y112", Icon::Light},  // by class
    {"Y12", Icon::TwinTurboprop},  // by class
    {"Y12F", Icon::Turboprop},  // by class
    {"Y130", Icon::BigBizJet},  // by class
    {"Y141", Icon::Falcon},  // by class
    {"Y18T", Icon::X_YK52},
    {"Y20", Icon::X_Y20},
    {"YA1", Icon::Light},  // by class
    {"YAK3", Icon::X_YAK3},
    {"YAK9", Icon::X_YAK3},
    {"YALE", Icon::X_DHC1},
    {"YARR", Icon::Light},  // by class
    {"YAST", Icon::Light},  // by class
    {"YC12", Icon::Light},  // by class
    {"YK11", Icon::X_YAK3},
    {"YK12", Icon::Light},  // by class
    {"YK13", Icon::X_M346},
    {"YK18", Icon::X_YK52},
    {"YK28", Icon::BigBizJet},  // by class
    {"YK3", Icon::X_YAK3},
    {"YK30", Icon::JetTrainer},  // by class
    {"YK38", Icon::Falcon},  // by class
    {"YK40", Icon::Falcon},  // by class
    {"YK42", Icon::X_YK42},
    {"YK50", Icon::X_SU26},
    {"YK52", Icon::X_YK52},
    {"YK53", Icon::Light},  // by class
    {"YK54", Icon::Light},  // by class
    {"YK55", Icon::X_SU26},
    {"YK58", Icon::Light},  // by class
    {"YL15", Icon::Light},  // by class
    {"YNHL", Icon::Heli4},  // by class
    {"YS11", Icon::Turboprop},  // by class
    {"YUKN", Icon::Light},  // by class
    {"YUNO", Icon::Light},  // by class
    {"YURO", Icon::BigBizJet},  // by class
    {"Z22", Icon::Light},  // by class
    {"Z26", Icon::Light},  // by class
    {"Z37P", Icon::Light},  // by class
    {"Z37T", Icon::TurbopropSingle},  // by class
    {"Z42", Icon::X_G115},
    {"Z43", Icon::Light},  // by class
    {"Z50", Icon::Light},  // by class
    {"ZA6", Icon::Heli2},  // by class
    {"ZEFR", Icon::Heli3},  // by class
    {"ZEP2", Icon::Light},  // by class
    {"ZEPH", Icon::Light},  // by class
    {"ZERO", Icon::X_ZERO},
    {"ZIA", Icon::Light},  // by class
    {"ZIU", Icon::Light},  // by class
    {"ZULU", Icon::Light},  // by class
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
    {"x_glider", 32},  // Glider
    {"x_balloon", 30},  // Airship
    {"b3", 20},  // Parachute
    {"b4", 28},  // Ultralight
    {"c0", 18},  // Ground
    {"dh8a", 40},  // Turboprop
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
    {"g_twin_piston", 32},  // TwinPiston
    {"g_twin_turboprop", 36},  // TwinTurboprop
    {"g_twin_high", 38},  // TwinHigh
    {"g_awacs", 50},  // Awacs
    {"g_hawkeye", 40},  // Hawkeye
    {"g_b52", 56},  // B52
    {"g_b1", 50},  // B1
    {"g_b2", 54},  // B2
    {"g_fighter_single", 34},  // FighterSingle
    {"g_fighter_twin", 38},  // FighterTwin
    {"g_fighter_delta", 36},  // FighterDelta
    {"g_jet_trainer", 32},  // JetTrainer
    {"g_heli_2", 32},  // Heli2
    {"g_heli_3", 34},  // Heli3
    {"g_heli_4", 36},  // Heli4
    {"g_heli_5", 40},  // Heli5
    {"g_heli_attack", 38},  // HeliAttack
    {"x_a318", 42},  // X_A318
    {"x_a319", 43},  // X_A319
    {"x_a321", 46},  // X_A321
    {"x_a220_100", 42},  // X_A220_100
    {"x_a220_300", 44},  // X_A220_300
    {"x_c919", 44},  // X_C919
    {"x_b732", 41},  // X_B732
    {"x_b733", 42},  // X_B733
    {"x_b734", 43},  // X_B734
    {"x_b735", 41},  // X_B735
    {"x_b736", 42},  // X_B736
    {"x_b737", 42},  // X_B737
    {"x_b739", 45},  // X_B739
    {"x_b752", 46},  // X_B752
    {"x_b753", 48},  // X_B753
    {"x_b762", 47},  // X_B762
    {"x_b764", 50},  // X_B764
    {"x_b772", 51},  // X_B772
    {"x_b773", 53},  // X_B773
    {"x_b77w", 53},  // X_B77W
    {"x_b779", 54},  // X_B779
    {"x_b788", 50},  // X_B788
    {"x_b78x", 52},  // X_B78X
    {"x_a300", 48},  // X_A300
    {"x_a310", 46},  // X_A310
    {"x_a332", 50},  // X_A332
    {"x_a339", 51},  // X_A339
    {"x_a359", 52},  // X_A359
    {"x_a35k", 53},  // X_A35K
    {"x_a346", 53},  // X_A346
    {"x_b748", 54},  // X_B748
    {"x_b74s", 50},  // X_B74S
    {"x_beluga", 49},  // X_BELUGA
    {"x_belugaxl", 51},  // X_BELUGAXL
    {"x_blcf", 53},  // X_BLCF
    {"x_dc10", 49},  // X_DC10
    {"x_l1011", 48},  // X_L1011
    {"x_b712", 43},  // X_B712
    {"x_md80", 46},  // X_MD80
    {"x_md90", 46},  // X_MD90
    {"x_dc9", 43},  // X_DC9
    {"x_f100", 43},  // X_F100
    {"x_f70", 41},  // X_F70
    {"x_b722", 46},  // X_B722
    {"x_b721", 44},  // X_B721
    {"x_bae146", 40},  // X_BAE146
    {"x_bae146_300", 41},  // X_BAE146_300
    {"x_t154", 47},  // X_T154
    {"x_il62", 48},  // X_IL62
    {"x_yk42", 43},  // X_YK42
    {"x_crj2", 39},  // X_CRJ2
    {"x_crj7", 41},  // X_CRJ7
    {"x_e170", 40},  // X_E170
    {"x_e175", 41},  // X_E175
    {"x_e190", 43},  // X_E190
    {"x_e290", 43},  // X_E290
    {"x_e135", 39},  // X_E135
    {"x_su95", 40},  // X_SU95
    {"x_arj21", 42},  // X_ARJ21
    {"x_dh8c", 39},  // X_DH8C
    {"x_dh8d", 42},  // X_DH8D
    {"x_atr42", 38},  // X_ATR42
    {"x_atr72", 39},  // X_ATR72
    {"x_sf34", 37},  // X_SF34
    {"x_sb20", 39},  // X_SB20
    {"x_d328", 37},  // X_D328
    {"x_j328", 37},  // X_J328
    {"x_e120", 36},  // X_E120
    {"x_js41", 36},  // X_JS41
    {"x_f50", 40},  // X_F50
    {"x_dhc7", 40},  // X_DHC7
    {"x_an26", 40},  // X_AN26
    {"x_c295", 39},  // X_C295
    {"x_cn235", 39},  // X_CN235
    {"x_c510", 32},  // X_C510
    {"x_c525", 32},  // X_C525
    {"x_c25a", 33},  // X_C25A
    {"x_c25b", 34},  // X_C25B
    {"x_c25c", 34},  // X_C25C
    {"x_c500", 34},  // X_C500
    {"x_c560", 34},  // X_C560
    {"x_c56x", 34},  // X_C56X
    {"x_c680", 36},  // X_C680
    {"x_c68a", 37},  // X_C68A
    {"x_c700", 37},  // X_C700
    {"x_c750", 37},  // X_C750
    {"x_lj35", 33},  // X_LJ35
    {"x_lj45", 35},  // X_LJ45
    {"x_lj60", 35},  // X_LJ60
    {"x_e50p", 31},  // X_E50P
    {"x_e55p", 34},  // X_E55P
    {"x_e545", 36},  // X_E545
    {"x_e550", 37},  // X_E550
    {"x_hdjt", 32},  // X_HDJT
    {"x_sf50", 31},  // X_SF50
    {"x_pc24", 34},  // X_PC24
    {"x_be40", 33},  // X_BE40
    {"x_prm1", 32},  // X_PRM1
    {"x_h25b", 33},  // X_H25B
    {"x_g280", 36},  // X_G280
    {"x_glf4", 39},  // X_GLF4
    {"x_glf6", 41},  // X_GLF6
    {"x_ga7c", 42},  // X_GA7C
    {"x_glex", 41},  // X_GLEX
    {"x_gl5t", 40},  // X_GL5T
    {"x_gl7t", 42},  // X_GL7T
    {"x_cl30", 36},  // X_CL30
    {"x_cl60", 36},  // X_CL60
    {"x_fa8x", 39},  // X_FA8X
    {"x_f2th", 37},  // X_F2TH
    {"x_fa50", 36},  // X_FA50
    {"x_fa6x", 39},  // X_FA6X
    {"x_fa20", 34},  // X_FA20
    {"x_c152", 29},  // X_C152
    {"x_c172", 30},  // X_C172
    {"x_c182", 30},  // X_C182
    {"x_c210", 30},  // X_C210
    {"x_c177", 30},  // X_C177
    {"x_c180", 30},  // X_C180
    {"x_pa18", 30},  // X_PA18
    {"x_j3", 30},  // X_J3
    {"x_bl8", 30},  // X_BL8
    {"x_husk", 30},  // X_HUSK
    {"x_m7", 29},  // X_M7
    {"x_ga8", 31},  // X_GA8
    {"x_pa32", 30},  // X_PA32
    {"x_pa38", 29},  // X_PA38
    {"x_pa24", 30},  // X_PA24
    {"x_pa46", 32},  // X_PA46
    {"x_sr22", 31},  // X_SR22
    {"x_da40", 31},  // X_DA40
    {"x_da20", 30},  // X_DA20
    {"x_be35", 29},  // X_BE35
    {"x_be36", 29},  // X_BE36
    {"x_m20", 30},  // X_M20
    {"x_rv7", 27},  // X_RV7
    {"x_rv10", 29},  // X_RV10
    {"x_rv12", 27},  // X_RV12
    {"x_lnc4", 28},  // X_LNC4
    {"x_pa28", 30},  // X_PA28
    {"x_tb20", 29},  // X_TB20
    {"x_dr40", 28},  // X_DR40
    {"x_g115", 29},  // X_G115
    {"x_aa5", 29},  // X_AA5
    {"x_tbm9", 31},  // X_TBM9
    {"x_kodi", 32},  // X_KODI
    {"x_pc6", 34},  // X_PC6
    {"x_dhc2", 33},  // X_DHC2
    {"x_dhc3", 35},  // X_DHC3
    {"x_c208", 34},  // X_C208
    {"x_pc12", 34},  // X_PC12
    {"x_at8t", 33},  // X_AT8T
    {"x_pa25", 30},  // X_PA25
    {"x_pa23", 30},  // X_PA23
    {"x_pa34", 31},  // X_PA34
    {"x_pa44", 31},  // X_PA44
    {"x_be58", 30},  // X_BE58
    {"x_be76", 30},  // X_BE76
    {"x_c310", 30},  // X_C310
    {"x_c340", 30},  // X_C340
    {"x_c414", 32},  // X_C414
    {"x_c421", 31},  // X_C421
    {"x_da42", 32},  // X_DA42
    {"x_da62", 33},  // X_DA62
    {"x_p68", 31},  // X_P68
    {"x_pa31", 31},  // X_PA31
    {"x_aest", 30},  // X_AEST
    {"x_b350", 35},  // X_B350
    {"x_be9l", 33},  // X_BE9L
    {"x_sw4", 35},  // X_SW4
    {"x_js31", 34},  // X_JS31
    {"x_mu2", 31},  // X_MU2
    {"x_c441", 33},  // X_C441
    {"x_pay3", 33},  // X_PAY3
    {"x_p180", 33},  // X_P180
    {"x_c337", 30},  // X_C337
    {"x_l410", 36},  // X_L410
    {"x_bn2", 33},  // X_BN2
    {"x_d228", 34},  // X_D228
    {"x_c212", 36},  // X_C212
    {"x_sc7", 36},  // X_SC7
    {"x_dc3", 40},  // X_DC3
    {"x_dc4", 43},  // X_DC4
    {"x_dc6", 43},  // X_DC6
    {"x_c46", 42},  // X_C46
    {"x_coni", 43},  // X_CONI
    {"x_l188", 41},  // X_L188
    {"x_visc", 40},  // X_VISC
    {"x_ju52", 40},  // X_JU52
    {"x_an2", 35},  // X_AN2
    {"x_be18", 33},  // X_BE18
    {"x_be17", 29},  // X_BE17
    {"x_g21", 33},  // X_G21
    {"x_pby", 41},  // X_PBY
    {"x_dhc6", 36},  // X_DHC6
    {"x_p51", 30},  // X_P51
    {"x_spit", 30},  // X_SPIT
    {"x_huri", 31},  // X_HURI
    {"x_me09", 29},  // X_ME09
    {"x_fw90", 30},  // X_FW90
    {"x_zero", 31},  // X_ZERO
    {"x_cors", 31},  // X_CORS
    {"x_p47", 31},  // X_P47
    {"x_p40", 30},  // X_P40
    {"x_p38", 34},  // X_P38
    {"x_b17", 41},  // X_B17
    {"x_b24", 42},  // X_B24
    {"x_b25", 36},  // X_B25
    {"x_b29", 45},  // X_B29
    {"x_lanc", 41},  // X_LANC
    {"x_mosq", 34},  // X_MOSQ
    {"x_t6", 31},  // X_T6
    {"x_t28", 31},  // X_T28
    {"x_stearman", 29},  // X_STEARMAN
    {"x_dh82", 28},  // X_DH82
    {"x_pitts", 25},  // X_PITTS
    {"x_yk52", 29},  // X_YK52
    {"x_a1sky", 33},  // X_A1SKY
    {"x_lgez", 27},  // X_LGEZ
    {"x_velo", 28},  // X_VELO
    {"x_e300", 27},  // X_E300
    {"x_su26", 27},  // X_SU26
    {"x_cp23", 27},  // X_CP23
    {"x_kitfox", 29},  // X_KITFOX
    {"x_xcub", 29},  // X_XCUB
    {"x_glasair", 27},  // X_GLASAIR
    {"x_la4", 30},  // X_LA4
    {"x_icona5", 30},  // X_ICONA5
    {"x_f86", 30},  // X_F86
    {"x_mg15", 29},  // X_MG15
    {"x_mg21", 33},  // X_MG21
    {"x_f104", 34},  // X_F104
    {"x_vamp", 30},  // X_VAMP
    {"x_hunt", 32},  // X_HUNT
    {"x_l39", 31},  // X_L39
    {"x_a4", 31},  // X_A4
    {"x_f4", 35},  // X_F4
    {"x_f14", 36},  // X_F14
    {"x_f15", 36},  // X_F15
    {"x_f18", 35},  // X_F18
    {"x_f22", 35},  // X_F22
    {"x_f35", 33},  // X_F35
    {"x_su27", 37},  // X_SU27
    {"x_mg29", 34},  // X_MG29
    {"x_tornado", 34},  // X_TORNADO
    {"x_jaguar", 34},  // X_JAGUAR
    {"x_mir2", 33},  // X_MIR2
    {"x_draken", 33},  // X_DRAKEN
    {"x_viggen", 34},  // X_VIGGEN
    {"x_rafale", 33},  // X_RAFALE
    {"x_gripen", 32},  // X_GRIPEN
    {"x_eufi", 34},  // X_EUFI
    {"x_j10", 34},  // X_J10
    {"x_f16", 33},  // X_F16
    {"x_f5", 33},  // X_F5
    {"x_hawk", 31},  // X_HAWK
    {"x_ajet", 31},  // X_AJET
    {"x_m346", 30},  // X_M346
    {"x_t7", 32},  // X_T7
    {"x_a10", 34},  // X_A10
    {"x_av8b", 32},  // X_AV8B
    {"x_f117", 36},  // X_F117
    {"x_sr71", 42},  // X_SR71
    {"x_u2", 41},  // X_U2
    {"x_c5", 53},  // X_C5
    {"x_a400", 46},  // X_A400
    {"x_p3", 43},  // X_P3
    {"x_c160", 44},  // X_C160
    {"x_an124", 53},  // X_AN124
    {"x_an225", 56},  // X_AN225
    {"x_il76", 47},  // X_IL76
    {"x_q4", 44},  // X_Q4
    {"x_mq9", 36},  // X_MQ9
    {"x_kc135", 45},  // X_KC135
    {"x_kc46", 47},  // X_KC46
    {"x_r22", 28},  // X_R22
    {"x_r44", 31},  // X_R44
    {"x_r66", 31},  // X_R66
    {"x_b06", 31},  // X_B06
    {"x_b505", 31},  // X_B505
    {"x_b47g", 32},  // X_B47G
    {"x_uh1", 34},  // X_UH1
    {"x_ah1", 35},  // X_AH1
    {"x_b222", 33},  // X_B222
    {"x_b407", 31},  // X_B407
    {"x_b412", 34},  // X_B412
    {"x_b429", 32},  // X_B429
    {"x_b525", 36},  // X_B525
    {"x_h500", 29},  // X_H500
    {"x_md900", 31},  // X_MD900
    {"x_as50", 31},  // X_AS50
    {"x_as55", 31},  // X_AS55
    {"x_ec30", 31},  // X_EC30
    {"x_ec20", 30},  // X_EC20
    {"x_ec35", 31},  // X_EC35
    {"x_ec45", 32},  // X_EC45
    {"x_as65", 32},  // X_AS65
    {"x_h160", 33},  // X_H160
    {"x_ec75", 35},  // X_EC75
    {"x_ec25", 36},  // X_EC25
    {"x_gazl", 31},  // X_GAZL
    {"x_alo3", 31},  // X_ALO3
    {"x_a109", 32},  // X_A109
    {"x_a139", 34},  // X_A139
    {"x_a169", 33},  // X_A169
    {"x_a189", 35},  // X_A189
    {"x_eh10", 37},  // X_EH10
    {"x_s76", 34},  // X_S76
    {"x_s92", 36},  // X_S92
    {"x_h60", 36},  // X_H60
    {"x_h53", 41},  // X_H53
    {"x_s61", 37},  // X_S61
    {"x_h47", 41},  // X_H47
    {"x_h46", 39},  // X_H46
    {"x_h64", 35},  // X_H64
    {"x_tigr", 33},  // X_TIGR
    {"x_a129", 32},  // X_A129
    {"x_mi8", 39},  // X_MI8
    {"x_mi24", 37},  // X_MI24
    {"x_mi26", 44},  // X_MI26
    {"x_mi28", 37},  // X_MI28
    {"x_ka52", 33},  // X_KA52
    {"x_ka32", 34},  // X_KA32
    {"x_nh90", 36},  // X_NH90
    {"x_lynx", 33},  // X_LYNX
    {"x_en28", 29},  // X_EN28
    {"x_h269", 29},  // X_H269
    {"x_g2ca", 27},  // X_G2CA
    {"x_a320", 43},  // X_A320
    {"x_b738", 44},  // X_B738
    {"x_b789", 51},  // X_B789
    {"x_crj9", 43},  // X_CRJ9
    {"x_b763", 49},  // X_B763
    {"x_a333", 51},  // X_A333
    {"x_e195", 44},  // X_E195
    {"x_e295", 45},  // X_E295
    {"x_e145", 40},  // X_E145
    {"x_b744", 52},  // X_B744
    {"x_a388", 54},  // X_A388
    {"x_a343", 51},  // X_A343
    {"x_md11", 50},  // X_MD11
    {"x_c17", 48},  // X_C17
    {"x_c130", 44},  // X_C130
    {"x_c27j", 40},  // X_C27J
    {"x_be20", 34},  // X_BE20
    {"x_b190", 35},  // X_B190
    {"x_fa7x", 39},  // X_FA7X
    {"x_glf5", 40},  // X_GLF5
    {"x_g150", 34},  // X_G150
    {"x_c650", 34},  // X_C650
    {"x_ea50", 30},  // X_EA50
    {"x_dh8a", 39},  // X_DH8A
    {"x_cl415", 40},  // X_CL415
    {"x_p06t", 30},  // X_P06T
    {"x_sling", 28},  // X_SLING
    {"x_balloon", 35},  // X_BALLOON
    {"x_blimp", 50},  // X_BLIMP
    {"x_gyro", 28},  // X_GYRO
    {"x_gyro_cabin", 28},  // X_GYRO_CABIN
    {"x_quad", 22},  // X_QUAD
    {"x_ppc", 30},  // X_PPC
    {"x_glider", 33},  // X_GLIDER
    {"x_glider2", 34},  // X_GLIDER2
    {"x_glider_open", 38},  // X_GLIDER_OPEN
    {"x_blanik", 34},  // X_BLANIK
    {"x_motorglider", 33},  // X_MOTORGLIDER
    {"x_trimotor", 38},  // X_TRIMOTOR
    {"x_x59", 40},  // X_X59
    {"x_ss2", 35},  // X_SS2
    {"x_whk2", 45},  // X_WHK2
    {"x_slch", 58},  // X_SLCH
    {"x_p82", 33},  // X_P82
    {"x_starship", 34},  // X_STARSHIP
    {"x_joby", 30},  // X_JOBY
    {"x_tb2", 31},  // X_TB2
    {"x_sgup", 47},  // X_SGUP
    {"x_c97", 45},  // X_C97
    {"x_dr1", 26},  // X_DR1
    {"x_camel", 28},  // X_CAMEL
    {"x_waco", 30},  // X_WACO
    {"x_g164", 30},  // X_G164
    {"x_sportbip", 27},  // X_SPORTBIP
    {"x_erco", 28},  // X_ERCO
    {"x_dfly", 25},  // X_DFLY
    {"x_dhc1", 30},  // X_DHC1
    {"x_t34", 29},  // X_T34
    {"x_storch", 32},  // X_STORCH
    {"x_lysa", 33},  // X_LYSA
    {"x_tbmav", 34},  // X_TBMAV
    {"x_hcat", 32},  // X_HCAT
    {"x_bcat", 30},  // X_BCAT
    {"x_sbd", 31},  // X_SBD
    {"x_a26", 37},  // X_A26
    {"x_b26m", 37},  // X_B26M
    {"x_p39", 29},  // X_P39
    {"x_yak3", 28},  // X_YAK3
    {"x_me262", 31},  // X_ME262
    {"x_t33", 31},  // X_T33
    {"x_a37", 30},  // X_A37
    {"x_jpro", 30},  // X_JPRO
    {"x_fouga", 31},  // X_FOUGA
    {"x_ov10", 32},  // X_OV10
    {"x_mohawk", 33},  // X_MOHAWK
    {"x_s2", 37},  // X_S2
    {"x_su25", 33},  // X_SU25
    {"x_f111", 37},  // X_F111
    {"x_a6", 34},  // X_A6
    {"x_f8", 34},  // X_F8
    {"x_tu95", 47},  // X_TU95
    {"x_c141", 48},  // X_C141
    {"x_c390", 43},  // X_C390
    {"x_dhc4", 40},  // X_DHC4
    {"x_c119", 42},  // X_C119
    {"x_an72", 41},  // X_AN72
    {"x_be200", 42},  // X_BE200
    {"x_t134", 43},  // X_T134
    {"x_dc8", 49},  // X_DC8
    {"x_ww24", 34},  // X_WW24
    {"x_sbr1", 32},  // X_SBR1
    {"x_ac50", 33},  // X_AC50
    {"x_rs12", 29},  // X_RS12
    {"x_aircam", 29},  // X_AIRCAM
    {"x_petrel", 29},  // X_PETREL
    {"x_y20", 47},  // X_Y20
    {"x_l29", 30},  // X_L29
    {"x_mrf1", 33},  // X_MRF1
    {"x_c408", 37},  // X_C408
    {"x_wilga", 30},  // X_WILGA
    {"x_p750", 31},  // X_P750
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
