#include "garage_hid.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_gap_bt_api.h"
#include "esp_hidd_api.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "GARAGE_HID";

#define HID_REPORT_ID_CONSUMER  1

#define HID_BIT_VOL_UP          (1u << 0)
#define HID_BIT_VOL_DOWN        (1u << 1)
#define HID_BIT_PLAY_PAUSE      (1u << 2)
#define HID_BIT_NEXT            (1u << 3)
#define HID_BIT_MUTE            (1u << 4)
#define HID_BIT_PREV            (1u << 5)

/*
   Consumer Control HID report - bitmask:
   bit0 = Volume Up
   bit1 = Volume Down
   bit2 = Play/Pause
   bit3 = Next Track
   bit4 = Mute
   bit5 = Previous Track
   bits6..15 padding
*/
static const uint8_t s_hid_descriptor[] = {
    0x05, 0x0C,        // Usage Page (Consumer)
    0x09, 0x01,        // Usage (Consumer Control)
    0xA1, 0x01,        // Collection (Application)
    0x85, HID_REPORT_ID_CONSUMER, // Report ID

    0x15, 0x00,        // Logical Minimum (0)
    0x25, 0x01,        // Logical Maximum (1)
    0x75, 0x01,        // Report Size (1)
    0x95, 0x06,        // Report Count (6)

    0x09, 0xE9,        // Usage (Volume Increment)
    0x09, 0xEA,        // Usage (Volume Decrement)
    0x09, 0xCD,        // Usage (Play/Pause)
    0x09, 0xB5,        // Usage (Scan Next Track)
    0x09, 0xE2,        // Usage (Mute)
    0x09, 0xB6,        // Usage (Scan Previous Track)
    0x81, 0x02,        // Input (Data,Var,Abs)

    0x95, 0x0A,        // Report Count (10)
    0x81, 0x03,        // Input (Const,Var,Abs) padding

    0xC0               // End Collection
};

static esp_hidd_qos_param_t s_qos;
static esp_hidd_app_param_t s_app_param;

static QueueHandle_t s_cmd_queue = NULL;

static bool s_started = false;
static bool s_registered = false;
static bool s_connected = false;
static bool s_has_last_bd_addr = false;
static esp_bd_addr_t s_last_bd_addr = {0};

static uint16_t cmd_to_bits(garage_hid_cmd_t cmd)
{
    switch (cmd) {
    case GARAGE_HID_VOL_UP:
        return HID_BIT_VOL_UP;
    case GARAGE_HID_VOL_DOWN:
        return HID_BIT_VOL_DOWN;
    case GARAGE_HID_PLAY_PAUSE:
        return HID_BIT_PLAY_PAUSE;
    case GARAGE_HID_NEXT:
        return HID_BIT_NEXT;
    case GARAGE_HID_PREV:
        return HID_BIT_PREV;
    case GARAGE_HID_MUTE:
        return HID_BIT_MUTE;
    default:
        return 0;
    }
}

static const char *cmd_name(garage_hid_cmd_t cmd)
{
    switch (cmd) {
    case GARAGE_HID_VOL_UP:
        return "VOL_UP";
    case GARAGE_HID_VOL_DOWN:
        return "VOL_DOWN";
    case GARAGE_HID_PLAY_PAUSE:
        return "PLAY_PAUSE";
    case GARAGE_HID_NEXT:
        return "NEXT";
    case GARAGE_HID_PREV:
        return "PREV";
    case GARAGE_HID_MUTE:
        return "MUTE";
    default:
        return "UNKNOWN";
    }
}

static void send_consumer_bits(uint16_t bits)
{
    uint8_t report[2] = {
        (uint8_t)(bits & 0xFF),
        (uint8_t)((bits >> 8) & 0xFF)
    };

    esp_err_t err = esp_bt_hid_device_send_report(
        ESP_HIDD_REPORT_TYPE_INTRDATA,
        HID_REPORT_ID_CONSUMER,
        sizeof(report),
        report
    );

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "send_report failed: %s", esp_err_to_name(err));
    }
}

static void hid_tx_task(void *arg)
{
    garage_hid_cmd_t cmd;

    while (1) {
        if (xQueueReceive(s_cmd_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            uint16_t bits = cmd_to_bits(cmd);

            if (bits == 0) {
                continue;
            }

            if (!s_registered) {
                ESP_LOGW(TAG, "HID not registered yet, drop %s", cmd_name(cmd));
                continue;
            }

            if (!s_connected) {
                ESP_LOGW(TAG, "HID not connected yet, drop %s", cmd_name(cmd));
                continue;
            }

            ESP_LOGI(TAG, "SEND %s bits=0x%04X", cmd_name(cmd), bits);

            send_consumer_bits(bits);
            vTaskDelay(pdMS_TO_TICKS(35));
            send_consumer_bits(0);
        }
    }
}

static void hidd_cb(esp_hidd_cb_event_t event, esp_hidd_cb_param_t *param)
{
    switch (event) {
    case ESP_HIDD_INIT_EVT:
        if (param->init.status == ESP_HIDD_SUCCESS) {
            ESP_LOGI(TAG, "HIDD init OK, registering app");

            memset(&s_qos, 0, sizeof(s_qos));
            memset(&s_app_param, 0, sizeof(s_app_param));

            s_app_param.name = "Garage Media Remote";
            s_app_param.description = "ESP32 Garage Media HID";
            s_app_param.provider = "Stickstoff";
            s_app_param.subclass = ESP_HID_CLASS_RMC;
            s_app_param.desc_list = (uint8_t *)s_hid_descriptor;
            s_app_param.desc_list_len = sizeof(s_hid_descriptor);

            esp_bt_hid_device_register_app(&s_app_param, &s_qos, &s_qos);
        } else {
            ESP_LOGE(TAG, "HIDD init failed, status=%d", param->init.status);
        }
        break;

    case ESP_HIDD_REGISTER_APP_EVT:
        if (param->register_app.status == ESP_HIDD_SUCCESS) {
            s_registered = true;
            ESP_LOGI(TAG, "HIDD register app OK");

            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

            if (param->register_app.in_use) {
                memcpy(s_last_bd_addr, param->register_app.bd_addr, sizeof(esp_bd_addr_t));
                s_has_last_bd_addr = true;

                ESP_LOGI(TAG, "HIDD virtual cable in use, reconnecting");
                esp_bt_hid_device_connect(param->register_app.bd_addr);
            }
        } else {
            ESP_LOGE(TAG, "HIDD register app failed, status=%d", param->register_app.status);
        }
        break;

    case ESP_HIDD_OPEN_EVT:
        if (param->open.status == ESP_HIDD_SUCCESS &&
            param->open.conn_status == ESP_HIDD_CONN_STATE_CONNECTED) {
            s_connected = true;
            memcpy(s_last_bd_addr, param->open.bd_addr, sizeof(esp_bd_addr_t));
            s_has_last_bd_addr = true;
            ESP_LOGI(TAG, "HIDD connected");
        } else {
            ESP_LOGW(TAG, "HIDD open evt status=%d conn_status=%d",
                     param->open.status, param->open.conn_status);
        }
        break;

    case ESP_HIDD_CLOSE_EVT:
        s_connected = false;
        ESP_LOGI(TAG, "HIDD disconnected/closed");
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
        break;

    case ESP_HIDD_SEND_REPORT_EVT:
        if (param->send_report.status != ESP_HIDD_SUCCESS) {
            ESP_LOGW(TAG, "HIDD send report failed status=%d reason=%d",
                     param->send_report.status, param->send_report.reason);
        }
        break;

    default:
        break;
    }
}

static void hid_init_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(4000));

    ESP_LOGI(TAG, "Starting Classic BT HID Device");

    esp_err_t err = esp_bt_hid_device_register_callback(hidd_cb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "register_callback failed: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }

    err = esp_bt_hid_device_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "hid_device_init failed: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }

    vTaskDelete(NULL);
}

void garage_hid_reconnect(void)
{
    ESP_LOGI(TAG, "Reconnect/pairing requested");

    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

    if (!s_registered) {
        ESP_LOGW(TAG, "HID not registered yet");
        return;
    }

    if (s_connected) {
        ESP_LOGI(TAG, "HID already connected");
        return;
    }

    if (s_has_last_bd_addr) {
        ESP_LOGI(TAG, "Trying HID reconnect to last phone");
        esp_bt_hid_device_connect(s_last_bd_addr);
    } else {
        ESP_LOGI(TAG, "No last HID phone stored, discoverable mode active");
    }
}

void garage_hid_start(void)
{
    if (s_started) {
        return;
    }

    s_started = true;

    s_cmd_queue = xQueueCreate(16, sizeof(garage_hid_cmd_t));
    if (!s_cmd_queue) {
        ESP_LOGE(TAG, "No RAM for HID queue");
        return;
    }

    xTaskCreate(hid_tx_task, "garage_hid_tx", 4096, NULL, 4, NULL);
    xTaskCreate(hid_init_task, "garage_hid_init", 4096, NULL, 3, NULL);

    ESP_LOGI(TAG, "HID start scheduled");
}

void garage_hid_send(garage_hid_cmd_t cmd)
{
    if (!s_cmd_queue) {
        ESP_LOGW(TAG, "HID queue not ready");
        return;
    }

    xQueueSend(s_cmd_queue, &cmd, 0);
}

