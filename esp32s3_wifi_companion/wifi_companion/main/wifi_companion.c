#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "driver/uart.h"
#include "string.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "driver/gpio.h"
#include "trainer_status.h"
#include "trainer_protocol.h"
#include "web_server.h"
#include "esp_log.h"
#include "esp_spiffs.h"

static const char *TAG = "wifi_companion";

#define WIFI_SSID      CONFIG_WIFI_SSID
#define WIFI_PASS      CONFIG_WIFI_PASS
#define UART_NUM       UART_NUM_0          // back to default UART0 for Teensy link
#define UART_RX_PIN    GPIO_NUM_44         // IO44 = UART0 RX (Unconnected. Console output.)
#define UART_TX_PIN    GPIO_NUM_43         // IO43 = UART0 TX (Unconnected. Console output.)
#define UART_1         UART_NUM_1          // back to default UART0 for Teensy link
#define UART_1_RX_PIN  GPIO_NUM_13         // IO13 = UART1 RX (connect to Teensy TX)
#define UART_1_TX_PIN  GPIO_NUM_12         // IO12 = UART1 TX (connect to Teensy RX)
#define BUF_LEN        512
#define READY_GPIO     GPIO_NUM_8   // Pin to signal readiness to Teensy
#define STATUS_LED_GPIO GPIO_NUM_2  // Built-in LED for connection status

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi started, connecting to AP...\n");
        esp_wifi_connect();
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi disconnected, retrying...");
        g_status.wifi_connected = false;
        g_status.teensy_ready = false;
        gpio_set_level(STATUS_LED_GPIO, 0);
        esp_wifi_connect();
    }
}

static void on_got_ip(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(TAG, "Connected with IP: " IPSTR, IP2STR(&event->ip_info.ip));
    g_status.wifi_connected = true;
    gpio_set_level(READY_GPIO, 0);  // signal ready to Teensy
}

static void wifi_init(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_cfg = { 0 };
    strcpy((char*)wifi_cfg.sta.ssid, WIFI_SSID);
    strcpy((char*)wifi_cfg.sta.password, WIFI_PASS);
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL);
    esp_wifi_start();
}

static void uart_init(void)
{
    uart_config_t config = {
        .baud_rate  = 115200,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE
    };
    // Keep console UART0 untouched (no driver install) to preserve ESP_LOG output
    // uart_driver_install(UART_NUM, BUF_LEN * 2, 0, 0, NULL, 0);
    // uart_param_config(UART_NUM, &config);
    // uart_set_pin(UART_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    uart_config_t config_1 = {
        .baud_rate  = 115200,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE
    };
    uart_driver_install(UART_1, BUF_LEN * 2, 0, 0, NULL, 0);
    uart_param_config(UART_1, &config_1);
    uart_set_pin(UART_1, UART_1_TX_PIN, UART_1_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

static void uart_num_task(void *arg)
{
    /* Allocate one persistent buffer on the task stack.  +1 for NUL so we never
       write past BUF_LEN even when uart_read_bytes() returns BUF_LEN bytes. */
    static uint8_t buf[BUF_LEN + 1];

    for (;;) {
        int len = uart_read_bytes(UART_NUM, buf, BUF_LEN, pdMS_TO_TICKS(20));
        if (len >= 0 && len <= BUF_LEN) buf[len] = '\0';
        if (len > 0) {
            // Trim leading/trailing CR/LF/space to make parsing robust
            while (len > 0 && (buf[len - 1] == '\r' || buf[len - 1] == '\n' || buf[len - 1] == ' ' || buf[len - 1] == '\t')) {
                buf[--len] = '\0';
            }
            size_t start = 0;
            while (start < len && (buf[start] == '\r' || buf[start] == '\n' || buf[start] == ' ' || buf[start] == '\t')) {
                start++;
            }
            printf("ESP32: %s\n", (char*)buf + start);
        }
    }
}

static void uart_1_task(void *arg)
{
    /* Stream-oriented newline splitter. */
    static char line[BUF_LEN + 1];
    static size_t line_len = 0;
    static uint8_t buf[BUF_LEN];

    for (;;) {
        int len = uart_read_bytes(UART_1, buf, BUF_LEN, pdMS_TO_TICKS(100));
        if (len <= 0) continue;
        ESP_LOGD("uart1", "read %d bytes", len);
        for (int i = 0; i < len; ++i) {
            char c = (char)buf[i];
            if (c == '\r') continue;              // ignore CR
            if (c == '\n') {
                if (line_len > 0) {
                    line[line_len] = '\0';       // NUL-terminate
                    process_teensy_message(line);
                    line_len = 0;                 // reset for next line
                }
            } else if (line_len < BUF_LEN) {
                line[line_len++] = c;
            } else {
                // overflow, reset
                line_len = 0;
            }
        }
    }
}

/*----------------------------------------------------------------------
 * LED status task – shows connection state
 *----------------------------------------------------------------------*/
static void status_led_task(void *arg)
{
    bool led_state = false;
    for (;;) {
        if (!g_status.wifi_connected) {
            /* Wi-Fi not connected – LED off */
            gpio_set_level(STATUS_LED_GPIO, 0);
            vTaskDelay(pdMS_TO_TICKS(500));
        } else if (g_status.wifi_connected && !g_status.teensy_ready) {
            /* Blink while waiting for Teensy READY */
            led_state = !led_state;
            gpio_set_level(STATUS_LED_GPIO, led_state);
            vTaskDelay(pdMS_TO_TICKS(200));
        } else {
            /* Solid on when both links are up */
            gpio_set_level(STATUS_LED_GPIO, 1);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

void mount_spiffs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "spiffs",
        .max_files = 5,
        .format_if_mount_failed = true
    };
    esp_vfs_spiffs_register(&conf);
}

void app_main(void)
{
    nvs_flash_init();

    // Keep global warning level to reduce noise but enable INFO for our module
    esp_log_level_set("*", ESP_LOG_INFO);
    /* Ensure our UART protocol logs are shown regardless of global level */
    esp_log_level_set("proto", ESP_LOG_INFO);
    //esp_log_level_set(TAG, ESP_LOG_INFO);
    mount_spiffs();
    wifi_init();
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL<<READY_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    gpio_set_level(READY_GPIO, 1);  // keep high until ready

    /* Configure status LED GPIO */
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL<<STATUS_LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&led_conf);
    gpio_set_level(STATUS_LED_GPIO, 0);

    uart_init();

    // Flush any residual bootloader/ROM noise that may still be in the UART RX FIFO
    // uart_flush_input(UART_NUM);  // driver not installed, avoid error
    vTaskDelay(pdMS_TO_TICKS(50));
    uart_flush_input(UART_1);
    vTaskDelay(pdMS_TO_TICKS(50));  // give line a moment to settle

    // Notify Teensy that ESP32 companion is ready
    const char ready_msg[] = MSG_READY_ESP32 "\n";
    uart_write_bytes(UART_1, ready_msg, strlen(ready_msg));

    // Start HTTP server once Wi-Fi is up (simple, assumes immediate connect)
    start_webserver();

    // uart_num_task removed – console UART0 now solely for logging
    xTaskCreate(uart_1_task, "uart_1_task", 4096, NULL, 12, NULL);
    /* LED status task (low priority) */
    xTaskCreate(status_led_task, "status_led", 1024, NULL, 1, NULL);

    // Optional: create additional tasks for web server / sockets etc.
}