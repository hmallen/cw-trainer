#include "trainer_protocol.h"
#include "driver/uart.h"  // for uart_wait_tx_idle_polling
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "driver/uart.h"
#include "esp_system.h"  // for esp_restart

#ifndef UART_1
#define UART_1 UART_NUM_1
#endif


trainer_status_t g_status;  // zero-initialised by default

void trainer_status_reset(void)
{
    memset(&g_status, 0, sizeof(g_status));
    g_status.frequency = 600;
    g_status.speed = 20;
    g_status.effective_speed = 13;
    strcpy(g_status.waveform, "Sine");
    strcpy(g_status.output, "Headphones");
    g_status.wifi_connected = false;
    g_status.teensy_ready = false;
}

static void parse_status_message(const char *status)
{
    const char *p = status;
    while (*p) {
        const char *eq = strchr(p, '=');
        if (!eq) break;
        const char *comma = strchr(eq, ',');
        if (!comma) comma = p + strlen(p);

        char key[16] = {0};
        char val[32] = {0};
        size_t klen = eq - p;
        size_t vlen = comma - eq - 1;
        if (klen >= sizeof(key)) klen = sizeof(key) - 1;
        if (vlen >= sizeof(val)) vlen = sizeof(val) - 1;
        memcpy(key, p, klen);
        memcpy(val, eq + 1, vlen);
        key[klen] = '\0';
        val[vlen] = '\0';

        if (strcmp(key, "LESSON") == 0) g_status.lesson = atoi(val);
        else if (strcmp(key, "FREQ") == 0) g_status.frequency = atoi(val);
        else if (strcmp(key, "SPEED") == 0) g_status.speed = atoi(val);
        else if (strcmp(key, "EFFSPEED") == 0) g_status.effective_speed = atoi(val);
        else if (strcmp(key, "ACC") == 0) g_status.accuracy = (float)atof(val);
        else if (strcmp(key, "DEC") == 0) g_status.decoder_enabled = (strcmp(val, "1") == 0);
        else if (strcmp(key, "KOCH") == 0) g_status.koch_mode = (strcmp(val, "1") == 0);
        else if (strcmp(key, "WAVE") == 0) strncpy(g_status.waveform, val, sizeof(g_status.waveform) - 1);
        else if (strcmp(key, "OUT") == 0) strncpy(g_status.output, val, sizeof(g_status.output) - 1);
        else if (strcmp(key, "SEND") == 0) g_status.sending = (strcmp(val, "1") == 0);
        else if (strcmp(key, "LISTEN") == 0) g_status.listening = (strcmp(val, "1") == 0);

        if (*comma == '\0') break;
        p = comma + 1;
    }
}

static void parse_stats_message(const char *stats)
{
    const char *p = stats;
    while (*p) {
        const char *eq = strchr(p, '=');
        if (!eq) break;
        const char *comma = strchr(eq, ',');
        if (!comma) comma = p + strlen(p);

        char key[16] = {0};
        char val[32] = {0};
        size_t klen = eq - p;
        size_t vlen = comma - eq - 1;
        if (klen >= sizeof(key)) klen = sizeof(key) - 1;
        if (vlen >= sizeof(val)) vlen = sizeof(val) - 1;
        memcpy(key, p, klen);
        memcpy(val, eq + 1, vlen);
        key[klen] = '\0';
        val[vlen] = '\0';

        if (strcmp(key, "SESSIONS") == 0) g_status.sessions = (uint32_t)atoi(val);
        else if (strcmp(key, "CHARS") == 0) g_status.characters = (uint32_t)atoi(val);
        else if (strcmp(key, "BESTWPM") == 0) g_status.best_wpm = (float)atof(val);

        if (*comma == '\0') break;
        p = comma + 1;
    }
}

static void append_decoded_text(const char *fragment)
{
    strncat(g_status.decoded_text, fragment, sizeof(g_status.decoded_text) - strlen(g_status.decoded_text) - 1);
    // keep last 200 chars like original
    size_t len = strlen(g_status.decoded_text);
    if (len > 200) {
        memmove(g_status.decoded_text, g_status.decoded_text + (len - 200), 200);
        g_status.decoded_text[200] = '\0';
    }
}

void process_teensy_message(const char *msg)
{
    ESP_LOGI("proto", "RX: %s", msg);
    if (strncmp(msg, "STATUS:", 7) == 0) {
        parse_status_message(msg + 7);
    } else if (strncmp(msg, "DECODED:", 8) == 0) {
        append_decoded_text(msg + 8);
    } else if (strncmp(msg, "CURRENT:", 8) == 0) {
        strncpy(g_status.current_text, msg + 8, sizeof(g_status.current_text) - 1);
    } else if (strncmp(msg, "STATS:", 6) == 0) {
        parse_stats_message(msg + 6);
    } else if (strncmp(msg, "PING", 4) == 0) {
        ESP_LOGI("proto", "PING received");
        /* Measure how long it takes from receiving PING to queueing the PONG
         * in the UART TX FIFO.  This helps identify latency that may make the
         * Teensy's heartbeat watchdog expire.  We also avoid the blocking
         * uart_wait_tx_idle_polling() call to keep the protocol handler
         * responsive. */
        uint32_t start_ts = esp_log_timestamp();
        uart_write_bytes(UART_1, "PONG\n", 5);    // async, do not block
        uint32_t latency = esp_log_timestamp() - start_ts;
        ESP_LOGI("proto", "TX: PONG (latency %u ms)", (unsigned)latency);
    } else if (strncmp(msg, "RESET_ESP", 9) == 0) {
        // Acknowledge then reboot
        uart_write_bytes(UART_1, "RESETTING\n", 10);
        fflush(stdout);
        esp_restart();
        uart_write_bytes(UART_1, "PONG\n", 5);
        // uart_wait_tx_idle_polling(UART_1);  // removed to avoid blocking
        ESP_LOGI("proto", "TX: PONG");
    } else if (strncmp(msg, "TEENSY:READY", 12) == 0) {
        //printf("Teensy ready\n");
        g_status.teensy_ready = true;
        uart_write_bytes(UART_1, "PONG\n", 5);
        // uart_wait_tx_idle_polling(UART_1);  // removed to avoid blocking
        ESP_LOGI("proto", "TX: PONG");
    }
}
