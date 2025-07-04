#ifndef TRAINER_CONSTANTS_H
#define TRAINER_CONSTANTS_H

#include <Arduino.h>
#include <pgmspace.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const char* const CALLSIGN_PREFIXES[] PROGMEM;
extern const uint8_t CALLSIGN_PREFIXES_COUNT;
extern const char* const CALLSIGN_SUFFIXES[] PROGMEM;
extern const uint8_t CALLSIGN_SUFFIXES_COUNT;
extern const char* const CONTEST_EXCHANGES[] PROGMEM;
extern const uint8_t CONTEST_EXCHANGES_COUNT;

#ifdef __cplusplus
}
#endif

#endif // TRAINER_CONSTANTS_H
