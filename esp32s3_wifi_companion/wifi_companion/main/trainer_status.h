#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Structure mirroring the fields used in the original ESP8266 sketch.
 * Sizes for text buffers are chosen to be generous yet reasonable for RAM.
 */
typedef struct {
    int lesson;
    int frequency;
    int speed;
    int effective_speed;
    float accuracy;
    bool decoder_enabled;
    bool koch_mode;

    char current_text[128];   // currently playing training text
    char decoded_text[256];   // rolling decoded buffer

    uint32_t sessions;
    uint32_t characters;
    float best_wpm;

    char waveform[16];
    char output[16];

    bool sending;
    bool listening;

    /* New connection-status flags */
    bool wifi_connected;   /* true once the ESP32 got an IP from AP */
    bool teensy_ready;     /* true once the Teensy sends TEENSY:READY */
} trainer_status_t;

/* Global instance that the rest of the program can reference */
extern trainer_status_t g_status;

/* Helper to restore defaults */
void trainer_status_reset(void);

#ifdef __cplusplus
}
#endif
