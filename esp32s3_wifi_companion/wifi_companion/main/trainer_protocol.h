#pragma once

/* Shared serial-protocol strings (kept in sync with Teensy firmware) */
#define MSG_PING        "PING"
#define MSG_PONG        "PONG"
#define MSG_READY_ESP01 "ESP01:READY"
#define MSG_READY_ESP32 "ESP32:READY"
#define MSG_HEARTBEAT   "ESP01:HEARTBEAT"

#define PREFIX_STATUS   "STATUS:"
#define PREFIX_STATS    "STATS:"
#define PREFIX_DECODED  "DECODED:"
#define PREFIX_CURRENT  "CURRENT:"

#include "trainer_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Parse a single newline-terminated message from the Teensy and update
 * global status accordingly. The call is safe to make from an ISR or task
 * context (no dynamic allocation inside except handled within protocol file).
 */
void process_teensy_message(const char *msg);

/* Reset g_status to default values. */
void trainer_status_reset(void);

#ifdef __cplusplus
}
#endif
