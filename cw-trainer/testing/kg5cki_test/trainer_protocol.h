#ifndef TRAINER_PROTOCOL_H
#define TRAINER_PROTOCOL_H

// Common serial-protocol strings shared by Teensy cw-trainer and ESP32 WiFi companion.
// Keeping them in one place prevents typos and mismatches.

// Basic keep-alive
#define MSG_PING               "PING"
#define MSG_PONG               "PONG"

// Ready notifications (both historical and current IDs)
#define MSG_READY_ESP01        "ESP01:READY"
#define MSG_READY_ESP32        "ESP32:READY"

// Legacy heartbeat (no longer used but kept for backward compatibility)
#define MSG_HEARTBEAT          "ESP01:HEARTBEAT"

// Message prefixes from Teensy to ESP32
#define PREFIX_STATUS          "STATUS:"
#define PREFIX_STATS           "STATS:"
#define PREFIX_DECODED         "DECODED:"
#define PREFIX_CURRENT         "CURRENT:"

#endif // TRAINER_PROTOCOL_H
