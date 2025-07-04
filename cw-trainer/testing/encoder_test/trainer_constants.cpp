#include "trainer_constants.h"

// ---- String literals in flash -----------------
const char p_K[] PROGMEM="K"; const char p_W[] PROGMEM="W"; const char p_N[] PROGMEM="N";
const char p_AA[] PROGMEM="AA"; const char p_AB[] PROGMEM="AB"; const char p_AC[] PROGMEM="AC";
const char p_AD[] PROGMEM="AD"; const char p_AE[] PROGMEM="AE"; const char p_AF[] PROGMEM="AF";
const char p_AG[] PROGMEM="AG"; const char p_AH[] PROGMEM="AH"; const char p_AI[] PROGMEM="AI";
const char p_AJ[] PROGMEM="AJ"; const char p_AK[] PROGMEM="AK"; const char p_AL[] PROGMEM="AL";
const char p_VE[] PROGMEM="VE"; const char p_VK[] PROGMEM="VK"; const char p_G[] PROGMEM="G";
const char p_DL[] PROGMEM="DL"; const char p_JA[] PROGMEM="JA"; const char p_HL[] PROGMEM="HL";
const char p_UA[] PROGMEM="UA";

const char s_A[] PROGMEM="A"; const char s_B[] PROGMEM="B"; const char s_C[] PROGMEM="C";
const char s_D[] PROGMEM="D"; const char s_E[] PROGMEM="E"; const char s_F[] PROGMEM="F";
const char s_G[] PROGMEM="G"; const char s_H[] PROGMEM="H"; const char s_I[] PROGMEM="I";
const char s_J[] PROGMEM="J"; const char s_K[] PROGMEM="K"; const char s_L[] PROGMEM="L";
const char s_M[] PROGMEM="M"; const char s_N[] PROGMEM="N"; const char s_O[] PROGMEM="O";
const char s_P[] PROGMEM="P"; const char s_Q[] PROGMEM="Q"; const char s_R[] PROGMEM="R";
const char s_S[] PROGMEM="S"; const char s_T[] PROGMEM="T"; const char s_U[] PROGMEM="U";
const char s_V[] PROGMEM="V"; const char s_W[] PROGMEM="W"; const char s_X[] PROGMEM="X";
const char s_Y[] PROGMEM="Y"; const char s_Z[] PROGMEM="Z";

const char ex_001[] PROGMEM="001"; const char ex_002[] PROGMEM="002"; const char ex_CA[] PROGMEM="CA";
const char ex_NY[] PROGMEM="NY"; const char ex_TX[] PROGMEM="TX"; const char ex_FL[] PROGMEM="FL";
const char ex_OH[] PROGMEM="OH"; const char ex_IL[] PROGMEM="IL"; const char ex_PA[] PROGMEM="PA";
const char ex_MI[] PROGMEM="MI";

// ---- tables ------------------------------
const char* const CALLSIGN_PREFIXES[] PROGMEM={p_K,p_W,p_N,p_AA,p_AB,p_AC,p_AD,p_AE,p_AF,p_AG,p_AH,p_AI,p_AJ,p_AK,p_AL,p_VE,p_VK,p_G,p_DL,p_JA,p_HL,p_UA};
const uint8_t CALLSIGN_PREFIXES_COUNT=sizeof(CALLSIGN_PREFIXES)/sizeof(CALLSIGN_PREFIXES[0]);

const char* const CALLSIGN_SUFFIXES[] PROGMEM={s_A,s_B,s_C,s_D,s_E,s_F,s_G,s_H,s_I,s_J,s_K,s_L,s_M,s_N,s_O,s_P,s_Q,s_R,s_S,s_T,s_U,s_V,s_W,s_X,s_Y,s_Z};
const uint8_t CALLSIGN_SUFFIXES_COUNT=sizeof(CALLSIGN_SUFFIXES)/sizeof(CALLSIGN_SUFFIXES[0]);

const char* const CONTEST_EXCHANGES[] PROGMEM={ex_001,ex_002,ex_CA,ex_NY,ex_TX,ex_FL,ex_OH,ex_IL,ex_PA,ex_MI};
const uint8_t CONTEST_EXCHANGES_COUNT=sizeof(CONTEST_EXCHANGES)/sizeof(CONTEST_EXCHANGES[0]);
