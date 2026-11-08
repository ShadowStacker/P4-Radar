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
    {"A1", Icon::X_A1SKY},
    {"A10", Icon::X_A10},
    {"A109", Icon::X_A109},
    {"A119", Icon::X_A109},
    {"A124", Icon::X_AN124},
    {"A129", Icon::X_A129},
    {"A139", Icon::X_A139},
    {"A149", Icon::X_A189},
    {"A159", Icon::X_LYNX},
    {"A169", Icon::X_A169},
    {"A189", Icon::X_A189},
    {"A19N", Icon::X_A319},
    {"A20N", Icon::X_A320},
    {"A21N", Icon::X_A321},
    {"A225", Icon::X_AN225},
    {"A306", Icon::X_A300},
    {"A30B", Icon::X_A300},
    {"A310", Icon::X_A310},
    {"A318", Icon::X_A318},
    {"A319", Icon::X_A319},
    {"A320", Icon::X_A320},
    {"A321", Icon::X_A321},
    {"A332", Icon::X_A332},
    {"A333", Icon::X_A333},
    {"A337", Icon::X_BELUGAXL},
    {"A338", Icon::X_A339},
    {"A339", Icon::X_A339},
    {"A342", Icon::X_A343},
    {"A343", Icon::X_A343},
    {"A345", Icon::X_A346},
    {"A346", Icon::X_A346},
    {"A359", Icon::X_A359},
    {"A35K", Icon::X_A35K},
    {"A388", Icon::X_A388},
    {"A3ST", Icon::X_BELUGA},
    {"A4", Icon::X_A4},
    {"A400", Icon::X_A400},
    {"A50", Icon::X_IL76},
    {"A6M", Icon::X_ZERO},
    {"AA1", Icon::X_AA5},
    {"AA5", Icon::X_AA5},
    {"AC11", Icon::TwinTurboprop},
    {"AC90", Icon::TwinTurboprop},
    {"AC95", Icon::TwinTurboprop},
    {"AEST", Icon::X_AEST},
    {"AH1", Icon::X_AH1},
    {"AH64", Icon::X_H64},
    {"AJ27", Icon::X_ARJ21},
    {"AJET", Icon::X_AJET},
    {"ALO2", Icon::X_ALO3},
    {"ALO3", Icon::X_ALO3},
    {"AMX", Icon::FighterSingle},
    {"AN12", Icon::X_C130},
    {"AN2", Icon::X_AN2},
    {"AN22", Icon::Freighter},
    {"AN24", Icon::X_AN26},
    {"AN26", Icon::X_AN26},
    {"AN28", Icon::TwinHigh},
    {"AN32", Icon::X_AN26},
    {"AS21", Icon::Glider},
    {"AS32", Icon::X_EC25},
    {"AS3B", Icon::X_EC25},
    {"AS50", Icon::X_AS50},
    {"AS55", Icon::X_AS55},
    {"AS65", Icon::X_AS65},
    {"ASK21", Icon::Glider},
    {"ASK3", Icon::Glider},
    {"ASTR", Icon::X_G150},
    {"ASW2", Icon::Glider},
    {"AT01", Icon::X_AT8T},
    {"AT3T", Icon::X_AT8T},
    {"AT43", Icon::X_ATR42},
    {"AT44", Icon::X_ATR42},
    {"AT45", Icon::X_ATR42},
    {"AT46", Icon::X_ATR42},
    {"AT4T", Icon::X_AT8T},
    {"AT5T", Icon::X_AT8T},
    {"AT6T", Icon::X_AT8T},
    {"AT72", Icon::X_ATR72},
    {"AT73", Icon::X_ATR72},
    {"AT75", Icon::X_ATR72},
    {"AT76", Icon::X_ATR72},
    {"AT8T", Icon::X_AT8T},
    {"AV8B", Icon::X_AV8B},
    {"B06", Icon::X_B06},
    {"B06T", Icon::X_B06},
    {"B1", Icon::B1},
    {"B17", Icon::X_B17},
    {"B190", Icon::X_B190},
    {"B2", Icon::B2},
    {"B204", Icon::X_UH1},
    {"B205", Icon::X_UH1},
    {"B212", Icon::X_UH1},
    {"B222", Icon::X_B222},
    {"B230", Icon::X_B222},
    {"B24", Icon::X_B24},
    {"B25", Icon::X_B25},
    {"B29", Icon::X_B29},
    {"B350", Icon::X_B350},
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
    {"B505", Icon::X_B505},
    {"B52", Icon::B52},
    {"B525", Icon::X_B525},
    {"B58T", Icon::X_BE58},
    {"B609", Icon::Tiltrotor},
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
    {"BALL", Icon::Airship},
    {"BCS1", Icon::X_A220_100},
    {"BCS3", Icon::X_A220_300},
    {"BE10", Icon::X_BE9L},
    {"BE17", Icon::X_BE17},
    {"BE18", Icon::X_BE18},
    {"BE19", Icon::SingleLow},
    {"BE20", Icon::X_BE20},
    {"BE23", Icon::SingleLow},
    {"BE24", Icon::SingleLow},
    {"BE30", Icon::X_B350},
    {"BE33", Icon::X_BE36},
    {"BE35", Icon::X_BE35},
    {"BE36", Icon::X_BE36},
    {"BE40", Icon::X_BE40},
    {"BE55", Icon::X_BE58},
    {"BE58", Icon::X_BE58},
    {"BE60", Icon::TwinPiston},
    {"BE76", Icon::X_BE76},
    {"BE95", Icon::X_BE58},
    {"BE99", Icon::TwinTurboprop},
    {"BE9L", Icon::X_BE9L},
    {"BE9T", Icon::X_BE9L},
    {"BK17", Icon::X_EC45},
    {"BL8", Icon::X_BL8},
    {"BLCF", Icon::X_BLCF},
    {"BN2P", Icon::X_BN2},
    {"BN2T", Icon::X_BN2},
    {"BT36", Icon::X_BE36},
    {"C130", Icon::X_C130},
    {"C135", Icon::X_KC135},
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
    {"C195", Icon::X_C180},
    {"C2", Icon::Hawkeye},
    {"C205", Icon::X_C182},
    {"C206", Icon::X_C182},
    {"C207", Icon::X_C182},
    {"C208", Icon::X_C208},
    {"C210", Icon::X_C210},
    {"C212", Icon::X_C212},
    {"C25A", Icon::X_C25A},
    {"C25B", Icon::X_C25B},
    {"C25C", Icon::X_C25C},
    {"C25M", Icon::X_C525},
    {"C27J", Icon::X_C27J},
    {"C295", Icon::X_C295},
    {"C303", Icon::X_C310},
    {"C30J", Icon::X_C130},
    {"C310", Icon::X_C310},
    {"C320", Icon::X_C310},
    {"C335", Icon::X_C340},
    {"C337", Icon::X_C337},
    {"C340", Icon::X_C340},
    {"C401", Icon::X_C414},
    {"C402", Icon::X_C414},
    {"C404", Icon::X_C421},
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
    {"C82R", Icon::X_C182},
    {"C82S", Icon::X_C182},
    {"C82T", Icon::X_C182},
    {"C919", Icon::X_C919},
    {"CAT", Icon::X_PBY},
    {"CC11", Icon::X_XCUB},
    {"CC18", Icon::X_XCUB},
    {"CC19", Icon::X_XCUB},
    {"CH47", Icon::X_H47},
    {"CH53", Icon::X_H53},
    {"CH60", Icon::X_KITFOX},
    {"CH70", Icon::X_KITFOX},
    {"CH7A", Icon::X_BL8},
    {"CH7B", Icon::X_BL8},
    {"CL2P", Icon::X_CL415},
    {"CL2T", Icon::X_CL415},
    {"CL30", Icon::X_CL30},
    {"CL35", Icon::X_CL30},
    {"CL4T", Icon::X_CL415},
    {"CL60", Icon::X_CL60},
    {"CN35", Icon::X_CN235},
    {"COL3", Icon::SingleLow},
    {"COL4", Icon::SingleLow},
    {"CONI", Icon::X_CONI},
    {"CORS", Icon::X_CORS},
    {"COZY", Icon::X_LGEZ},
    {"CP10", Icon::X_CP23},
    {"CP23", Icon::X_CP23},
    {"CRJ1", Icon::X_CRJ2},
    {"CRJ2", Icon::X_CRJ2},
    {"CRJ7", Icon::X_CRJ7},
    {"CRJ9", Icon::X_CRJ9},
    {"CRJX", Icon::X_CRJ9},
    {"CRUZ", Icon::SingleLow},
    {"D228", Icon::X_D228},
    {"D328", Icon::X_D328},
    {"DA20", Icon::X_DA20},
    {"DA40", Icon::X_DA40},
    {"DA42", Icon::X_DA42},
    {"DA62", Icon::X_DA62},
    {"DC10", Icon::X_DC10},
    {"DC3", Icon::X_DC3},
    {"DC3T", Icon::X_DC3},
    {"DC4", Icon::X_DC4},
    {"DC6", Icon::X_DC6},
    {"DC7", Icon::X_DC6},
    {"DC91", Icon::X_DC9},
    {"DC92", Icon::X_DC9},
    {"DC93", Icon::X_DC9},
    {"DC94", Icon::X_DC9},
    {"DC95", Icon::X_DC9},
    {"DG1T", Icon::Glider},
    {"DH3T", Icon::X_DHC3},
    {"DH82", Icon::X_DH82},
    {"DH8A", Icon::X_DH8A},
    {"DH8B", Icon::X_DH8A},
    {"DH8C", Icon::X_DH8C},
    {"DH8D", Icon::X_DH8D},
    {"DHC2", Icon::X_DHC2},
    {"DHC3", Icon::X_DHC3},
    {"DHC6", Icon::X_DHC6},
    {"DHC7", Icon::X_DHC7},
    {"DIMO", Icon::Glider},
    {"DISC", Icon::Glider},
    {"DO28", Icon::TwinHigh},
    {"DR40", Icon::X_DR40},
    {"DRAK", Icon::X_DRAKEN},
    {"DUOD", Icon::Glider},
    {"DV20", Icon::X_DA20},
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
    {"E290", Icon::X_E290},
    {"E295", Icon::X_E295},
    {"E300", Icon::X_E300},
    {"E314", Icon::TurbopropSingle},
    {"E330", Icon::X_E300},
    {"E35L", Icon::X_E135},
    {"E3CF", Icon::Awacs},
    {"E3TF", Icon::Awacs},
    {"E45X", Icon::X_E145},
    {"E50P", Icon::X_E50P},
    {"E545", Icon::X_E545},
    {"E550", Icon::X_E550},
    {"E55P", Icon::X_E55P},
    {"E6", Icon::X_KC135},
    {"E737", Icon::X_B737},
    {"E75L", Icon::X_E175},
    {"E75S", Icon::X_E175},
    {"E767", Icon::Awacs},
    {"EA18", Icon::X_F18},
    {"EA50", Icon::X_EA50},
    {"EC20", Icon::X_EC20},
    {"EC25", Icon::X_EC25},
    {"EC30", Icon::X_EC30},
    {"EC35", Icon::X_EC35},
    {"EC45", Icon::X_EC45},
    {"EC55", Icon::X_AS65},
    {"EC75", Icon::X_EC75},
    {"ECHO", Icon::X_KITFOX},
    {"EH10", Icon::X_EH10},
    {"EN28", Icon::X_EN28},
    {"EN48", Icon::X_EN28},
    {"EPIC", Icon::X_TBM9},
    {"EUFI", Icon::X_EUFI},
    {"EVOT", Icon::X_LNC4},
    {"EXPL", Icon::X_MD900},
    {"F100", Icon::X_F100},
    {"F104", Icon::X_F104},
    {"F117", Icon::X_F117},
    {"F14", Icon::X_F14},
    {"F15", Icon::X_F15},
    {"F16", Icon::X_F16},
    {"F18", Icon::X_F18},
    {"F18S", Icon::X_F18},
    {"F2", Icon::X_F16},
    {"F22", Icon::X_F22},
    {"F260", Icon::SingleLow},
    {"F27", Icon::X_F50},
    {"F2TH", Icon::X_F2TH},
    {"F35", Icon::X_F35},
    {"F4", Icon::X_F4},
    {"F406", Icon::TwinTurboprop},
    {"F4U", Icon::X_CORS},
    {"F5", Icon::X_F5},
    {"F50", Icon::X_F50},
    {"F70", Icon::X_F70},
    {"F86", Icon::X_F86},
    {"F900", Icon::X_FA50},
    {"FA10", Icon::X_FA20},
    {"FA20", Icon::X_FA20},
    {"FA50", Icon::X_FA50},
    {"FA6X", Icon::X_FA6X},
    {"FA7X", Icon::X_FA7X},
    {"FA8X", Icon::X_FA8X},
    {"FW90", Icon::X_FW90},
    {"G115", Icon::X_G115},
    {"G120", Icon::X_G115},
    {"G150", Icon::X_G150},
    {"G21", Icon::X_G21},
    {"G280", Icon::X_G280},
    {"G2CA", Icon::X_G2CA},
    {"G44", Icon::X_G21},
    {"GA5C", Icon::X_GA7C},
    {"GA6C", Icon::X_GA7C},
    {"GA7C", Icon::X_GA7C},
    {"GA8", Icon::X_GA8},
    {"GA8C", Icon::X_GA7C},
    {"GALX", Icon::X_G150},
    {"GAZL", Icon::X_GAZL},
    {"GC1", Icon::X_AA5},
    {"GL5T", Icon::X_GL5T},
    {"GL7T", Icon::X_GL7T},
    {"GL8T", Icon::X_GL7T},
    {"GLAS", Icon::X_GLASAIR},
    {"GLEX", Icon::X_GLEX},
    {"GLF2", Icon::X_GLF5},
    {"GLF3", Icon::X_GLF5},
    {"GLF4", Icon::X_GLF4},
    {"GLF5", Icon::X_GLF5},
    {"GLF6", Icon::X_GLF6},
    {"GLID", Icon::Glider},
    {"GLIM", Icon::Glider},
    {"GLST", Icon::X_GLASAIR},
    {"GND", Icon::Ground},
    {"GRIF", Icon::X_GRIPEN},
    {"H1", Icon::X_AH1},
    {"H125", Icon::X_AS50},
    {"H130", Icon::X_EC30},
    {"H135", Icon::X_EC35},
    {"H145", Icon::X_EC45},
    {"H155", Icon::X_AS65},
    {"H160", Icon::X_H160},
    {"H175", Icon::X_EC75},
    {"H215", Icon::X_EC25},
    {"H225", Icon::X_EC25},
    {"H25A", Icon::X_H25B},
    {"H25B", Icon::X_H25B},
    {"H25C", Icon::X_H25B},
    {"H269", Icon::X_H269},
    {"H3", Icon::X_S61},
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
    {"HAR", Icon::X_AV8B},
    {"HAR2", Icon::X_AV8B},
    {"HAWK", Icon::X_HAWK},
    {"HDJT", Icon::X_HDJT},
    {"HRON", Icon::X_MQ9},
    {"HUNT", Icon::X_HUNT},
    {"HURI", Icon::X_HURI},
    {"HUSK", Icon::X_HUSK},
    {"ICON", Icon::X_ICONA5},
    {"IL14", Icon::Turboprop},
    {"IL18", Icon::X_C130},
    {"IL62", Icon::X_IL62},
    {"IL76", Icon::X_IL76},
    {"J10", Icon::X_J10},
    {"J11", Icon::X_SU27},
    {"J15", Icon::X_SU27},
    {"J20", Icon::FighterTwin},
    {"J3", Icon::X_J3},
    {"J328", Icon::X_J328},
    {"JAGR", Icon::X_JAGUAR},
    {"JANU", Icon::Glider},
    {"JAS3", Icon::X_GRIPEN},
    {"JF17", Icon::FighterSingle},
    {"JS31", Icon::X_JS31},
    {"JS32", Icon::X_JS31},
    {"JS41", Icon::X_JS41},
    {"JU52", Icon::X_JU52},
    {"K35E", Icon::X_KC135},
    {"K35R", Icon::X_KC135},
    {"K35T", Icon::X_KC135},
    {"K46", Icon::X_KC46},
    {"K8", Icon::JetTrainer},
    {"KA26", Icon::X_KA32},
    {"KA27", Icon::X_KA32},
    {"KA32", Icon::X_KA32},
    {"KA50", Icon::X_KA52},
    {"KA52", Icon::X_KA52},
    {"KC46", Icon::X_KC46},
    {"KFIR", Icon::FighterDelta},
    {"KFOX", Icon::X_KITFOX},
    {"KODI", Icon::X_KODI},
    {"L049", Icon::X_CONI},
    {"L101", Icon::X_L1011},
    {"L1049", Icon::X_CONI},
    {"L159", Icon::X_L39},
    {"L188", Icon::X_L188},
    {"L39", Icon::X_L39},
    {"L410", Icon::X_L410},
    {"L749", Icon::X_CONI},
    {"LA25", Icon::X_LA4},
    {"LA4", Icon::X_LA4},
    {"LAMA", Icon::X_ALO3},
    {"LANC", Icon::X_LANC},
    {"LGEZ", Icon::X_LGEZ},
    {"LJ31", Icon::X_LJ35},
    {"LJ35", Icon::X_LJ35},
    {"LJ36", Icon::X_LJ35},
    {"LJ40", Icon::X_LJ45},
    {"LJ45", Icon::X_LJ45},
    {"LJ55", Icon::X_LJ60},
    {"LJ60", Icon::X_LJ60},
    {"LJ70", Icon::X_LJ45},
    {"LJ75", Icon::X_LJ45},
    {"LNC2", Icon::X_LNC4},
    {"LNC4", Icon::X_LNC4},
    {"LS8", Icon::Glider},
    {"LYNX", Icon::X_LYNX},
    {"M2000", Icon::X_MIR2},
    {"M20J", Icon::X_M20},
    {"M20K", Icon::X_M20},
    {"M20P", Icon::X_M20},
    {"M20R", Icon::X_M20},
    {"M20T", Icon::X_M20},
    {"M28", Icon::TwinHigh},
    {"M346", Icon::X_M346},
    {"M5", Icon::X_M7},
    {"M6", Icon::X_M7},
    {"M7", Icon::X_M7},
    {"M700", Icon::X_PA46},
    {"MA60", Icon::X_AN26},
    {"MD11", Icon::X_MD11},
    {"MD60", Icon::X_MD900},
    {"MD81", Icon::X_MD80},
    {"MD82", Icon::X_MD80},
    {"MD83", Icon::X_MD80},
    {"MD87", Icon::X_MD80},
    {"MD88", Icon::X_MD80},
    {"MD90", Icon::X_MD90},
    {"MD90X", Icon::X_MD900},
    {"ME09", Icon::X_ME09},
    {"MG15", Icon::X_MG15},
    {"MG17", Icon::X_MG15},
    {"MG21", Icon::X_MG21},
    {"MG29", Icon::X_MG29},
    {"MG31", Icon::FighterTwin},
    {"MG35", Icon::X_MG29},
    {"MH60", Icon::X_H60},
    {"MI14", Icon::X_MI8},
    {"MI17", Icon::X_MI8},
    {"MI24", Icon::X_MI24},
    {"MI26", Icon::X_MI26},
    {"MI28", Icon::X_MI28},
    {"MI35", Icon::X_MI24},
    {"MI8", Icon::X_MI8},
    {"MIR2", Icon::X_MIR2},
    {"MIR3", Icon::X_MIR2},
    {"MOSQ", Icon::X_MOSQ},
    {"MQ1", Icon::X_MQ9},
    {"MQ9", Icon::X_MQ9},
    {"MT7", Icon::X_M7},
    {"MU2", Icon::X_MU2},
    {"NH90", Icon::X_NH90},
    {"NIMB", Icon::Glider},
    {"P06T", Icon::X_P06T},
    {"P180", Icon::X_P180},
    {"P210", Icon::X_C210},
    {"P28A", Icon::X_PA28},
    {"P28B", Icon::X_PA28},
    {"P28R", Icon::X_PA28},
    {"P28T", Icon::X_PA28},
    {"P28U", Icon::X_PA28},
    {"P3", Icon::X_P3},
    {"P32R", Icon::X_PA32},
    {"P32T", Icon::X_PA32},
    {"P38", Icon::X_P38},
    {"P40", Icon::X_P40},
    {"P46T", Icon::X_PA46},
    {"P47", Icon::X_P47},
    {"P51", Icon::X_P51},
    {"P68", Icon::X_P68},
    {"P8", Icon::X_B738},
    {"P92", Icon::X_KITFOX},
    {"PA12", Icon::X_PA18},
    {"PA14", Icon::X_PA18},
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
    {"PIAT", Icon::Ultralight},
    {"PRM1", Icon::X_PRM1},
    {"PTS1", Icon::X_PITTS},
    {"PTS2", Icon::X_PITTS},
    {"Q1", Icon::X_MQ9},
    {"Q4", Icon::X_Q4},
    {"Q9", Icon::X_MQ9},
    {"R135", Icon::X_KC135},
    {"R22", Icon::X_R22},
    {"R44", Icon::X_R44},
    {"R66", Icon::X_R66},
    {"RFAL", Icon::X_RAFALE},
    {"RJ1H", Icon::X_BAE146_300},
    {"RJ70", Icon::X_BAE146},
    {"RJ85", Icon::X_BAE146},
    {"RQ4", Icon::X_Q4},
    {"RV10", Icon::X_RV10},
    {"RV12", Icon::X_RV12},
    {"RV14", Icon::X_RV7},
    {"RV4", Icon::X_RV7},
    {"RV6", Icon::X_RV7},
    {"RV7", Icon::X_RV7},
    {"RV8", Icon::X_RV7},
    {"RV9", Icon::X_RV7},
    {"S22T", Icon::X_SR22},
    {"S300", Icon::X_H269},
    {"S61", Icon::X_S61},
    {"S70", Icon::X_H60},
    {"S76", Icon::X_S76},
    {"S92", Icon::X_S92},
    {"SAVG", Icon::X_KITFOX},
    {"SB20", Icon::X_SB20},
    {"SC7", Icon::X_SC7},
    {"SERV", Icon::Ground},
    {"SF34", Icon::X_SF34},
    {"SF50", Icon::X_SF50},
    {"SH33", Icon::X_SC7},
    {"SH36", Icon::X_SC7},
    {"SHIP", Icon::Airship},
    {"SIRA", Icon::X_RV12},
    {"SLG2", Icon::X_SLING},
    {"SLG4", Icon::X_SLING},
    {"SLGT", Icon::X_SLING},
    {"SPIT", Icon::X_SPIT},
    {"SR20", Icon::X_SR22},
    {"SR22", Icon::X_SR22},
    {"SR71", Icon::X_SR71},
    {"SRAY", Icon::X_ICONA5},
    {"ST75", Icon::X_STEARMAN},
    {"SU25", Icon::FighterSingle},
    {"SU26", Icon::X_SU26},
    {"SU27", Icon::X_SU27},
    {"SU29", Icon::X_SU26},
    {"SU30", Icon::X_SU27},
    {"SU31", Icon::X_SU26},
    {"SU33", Icon::X_SU27},
    {"SU34", Icon::FighterTwin},
    {"SU35", Icon::X_SU27},
    {"SU57", Icon::FighterTwin},
    {"SU95", Icon::X_SU95},
    {"SW2", Icon::X_SW4},
    {"SW3", Icon::X_SW4},
    {"SW4", Icon::X_SW4},
    {"T154", Icon::X_T154},
    {"T206", Icon::X_C182},
    {"T210", Icon::X_C210},
    {"T28", Icon::X_T28},
    {"T34", Icon::TurbopropSingle},
    {"T38", Icon::X_F5},
    {"T45", Icon::X_HAWK},
    {"T50", Icon::JetTrainer},
    {"T6", Icon::X_T6},
    {"T7", Icon::X_T7},
    {"TB10", Icon::X_TB20},
    {"TB20", Icon::X_TB20},
    {"TB21", Icon::X_TB20},
    {"TB9", Icon::X_TB20},
    {"TBM7", Icon::X_TBM9},
    {"TBM8", Icon::X_TBM9},
    {"TBM9", Icon::X_TBM9},
    {"TEX2", Icon::TurbopropSingle},
    {"TIGR", Icon::X_TIGR},
    {"TOBA", Icon::X_TB20},
    {"TOR", Icon::X_TORNADO},
    {"TWR", Icon::Ground},
    {"U2", Icon::X_U2},
    {"UH1", Icon::X_UH1},
    {"UH60", Icon::X_H60},
    {"ULAC", Icon::Ultralight},
    {"V22", Icon::Tiltrotor},
    {"VAMP", Icon::X_VAMP},
    {"VELO", Icon::X_VELO},
    {"VENT", Icon::Glider},
    {"VEZE", Icon::X_LGEZ},
    {"VIGN", Icon::X_VIGGEN},
    {"VISC", Icon::X_VISC},
    {"VL3", Icon::X_RV12},
    {"XCUB", Icon::X_XCUB},
    {"YK13", Icon::X_M346},
    {"YK18", Icon::X_YK52},
    {"YK42", Icon::X_YK42},
    {"YK52", Icon::X_YK52},
    {"ZERO", Icon::X_ZERO},
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
    {"dh8a", 40},  // Turboprop
    {"heavyfreighter", 54},  // Freighter
    {"tiltrotorcraft", 40},  // Tiltrotor
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
    {"g_heli_4", 36},  // Heli4
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
