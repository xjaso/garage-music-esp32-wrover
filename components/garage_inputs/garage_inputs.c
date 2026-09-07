#include "garage_inputs.h"
#include "garage_hid.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "GARAGE_INPUT";

/*
   Final zapojenie:
   ENC_A    GPIO32
   ENC_B    GPIO33
   ENC_SW   GPIO14

   NEXT     GPIO19
   PLAY     GPIO25

   Vsetko active-low: pin -> tlacidlo -> GND
   PWR_OFF  GPIO27 -> Pololu OFF (aktivny HIGH pulz)
*/

#define ENC_A       GPIO_NUM_32
#define ENC_B       GPIO_NUM_33
#define ENC_SW      GPIO_NUM_14

#define BTN_NEXT    GPIO_NUM_19
#define BTN_PLAY    GPIO_NUM_25
#define PWR_OFF     GPIO_NUM_27
#define LCD_BL      GPIO_NUM_13

#define DEBOUNCE_MS     90
#define HOLD_MS         900
#define LOCKOUT_MS      250
#define LOOP_MS         10

typedef struct {
    gpio_num_t pin;
    const char *name;

    int raw_last;
    int stable;
    uint32_t raw_changed_ms;

    bool pressed;
    uint32_t press_start_ms;
    uint32_t last_event_ms;
    bool hold_sent;
} button_t;

static bool s_started = false;

static uint32_t ms_now(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static int read_pin(gpio_num_t pin)
{
    return gpio_get_level(pin);
}

static void button_init(button_t *b, gpio_num_t pin, const char *name)
{
    int v = read_pin(pin);

    memset(b, 0, sizeof(*b));
    b->pin = pin;
    b->name = name;
    b->raw_last = v;
    b->stable = v;
    b->raw_changed_ms = ms_now();
}

static void emit_click(const char *name)
{
    ESP_LOGI(TAG, "%s CLICK", name);

    if (strcmp(name, "ENC") == 0) {
        garage_hid_send(GARAGE_HID_PLAY_PAUSE);
    } else if (strcmp(name, "NEXT") == 0) {
        garage_hid_send(GARAGE_HID_NEXT);
    } else if (strcmp(name, "PLAY") == 0) {
        garage_hid_send(GARAGE_HID_PREV);
    }
}

static void garage_pololu_power_off(void)
{
    ESP_LOGW(TAG, "POWER OFF: display off, Pololu OFF pulse on GPIO27");

    // Zhasni podsvietenie este pred odpojenim napajania.
    gpio_set_level(LCD_BL, 0);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Pololu 2808 OFF je aktivny HIGH (>1 V).
    gpio_set_level(PWR_OFF, 1);
    vTaskDelay(pdMS_TO_TICKS(300));

    // Pri napajani cez Pololu sa sem uz ESP vacsinou nedostane.
    // Pri teste cez USB vrat pin do LOW, aby OFF nezostal aktivny.
    gpio_set_level(PWR_OFF, 0);
    ESP_LOGW(TAG, "Pololu OFF pulse finished - power was not removed (USB/backfeed?)");
}

static void emit_hold(const char *name)
{
    ESP_LOGI(TAG, "%s HOLD", name);

    if (strcmp(name, "ENC") == 0) {
        garage_hid_send(GARAGE_HID_MUTE);
    } else if (strcmp(name, "NEXT") == 0) {
        ESP_LOGI(TAG, "NEXT HOLD -> reconnect/pairing");
        garage_hid_reconnect();
    } else if (strcmp(name, "PLAY") == 0) {
        ESP_LOGI(TAG, "PLAY HOLD -> real power off via Pololu");
        garage_pololu_power_off();
    }
}

static void button_poll(button_t *b)
{
    uint32_t now = ms_now();
    int raw = read_pin(b->pin);

    if (raw != b->raw_last) {
        b->raw_last = raw;
        b->raw_changed_ms = now;
        return;
    }

    if ((now - b->raw_changed_ms) < DEBOUNCE_MS) {
        return;
    }

    if (raw == b->stable) {
        if (b->pressed && !b->hold_sent && (now - b->press_start_ms) >= HOLD_MS) {
            if ((now - b->last_event_ms) >= LOCKOUT_MS) {
                b->hold_sent = true;
                b->last_event_ms = now;
                emit_hold(b->name);
            }
        }
        return;
    }

    b->stable = raw;

    if (b->stable == 0) {
        b->pressed = true;
        b->press_start_ms = now;
        b->hold_sent = false;
    } else {
        if (b->pressed) {
            uint32_t held = now - b->press_start_ms;

            if (!b->hold_sent && held < HOLD_MS && (now - b->last_event_ms) >= LOCKOUT_MS) {
                b->last_event_ms = now;
                emit_click(b->name);
            }

            b->pressed = false;
            b->hold_sent = false;
        }
    }
}

static const int8_t encoder_table[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
};

static void inputs_task(void *arg)
{
    button_t enc_sw;
    button_t next_btn;
    button_t play_btn;

    button_init(&enc_sw, ENC_SW, "ENC");
    button_init(&next_btn, BTN_NEXT, "NEXT");
    button_init(&play_btn, BTN_PLAY, "PLAY");

    int enc_last = (read_pin(ENC_A) << 1) | read_pin(ENC_B);
    int enc_accum = 0;

    while (1) {
        int enc_now = (read_pin(ENC_A) << 1) | read_pin(ENC_B);
        int idx = ((enc_last & 0x03) << 2) | (enc_now & 0x03);
        int8_t step = encoder_table[idx];

        if (step != 0) {
            enc_accum += step;

            if (enc_accum >= 4) {
                enc_accum = 0;
                ESP_LOGI(TAG, "ENC RIGHT");
                garage_hid_send(GARAGE_HID_VOL_UP);
            } else if (enc_accum <= -4) {
                enc_accum = 0;
                ESP_LOGI(TAG, "ENC LEFT");
                garage_hid_send(GARAGE_HID_VOL_DOWN);
            }
        }

        enc_last = enc_now;

        button_poll(&enc_sw);
        button_poll(&next_btn);
        button_poll(&play_btn);

        vTaskDelay(pdMS_TO_TICKS(LOOP_MS));
    }
}

void garage_inputs_start(void)
{
    if (s_started) return;
    s_started = true;

    uint64_t mask =
        (1ULL << ENC_A) |
        (1ULL << ENC_B) |
        (1ULL << ENC_SW) |
        (1ULL << BTN_NEXT) |
        (1ULL << BTN_PLAY);

    gpio_config_t io_conf = {
        .pin_bit_mask = mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    gpio_config(&io_conf);

    // Pololu OFF control: normal state LOW, short HIGH pulse requests shutdown.
    gpio_config_t pwr_conf = {
        .pin_bit_mask = (1ULL << PWR_OFF),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&pwr_conf);
    gpio_set_level(PWR_OFF, 0);

    gpio_set_direction(LCD_BL, GPIO_MODE_OUTPUT);
    gpio_set_level(LCD_BL, 1);

    ESP_LOGI(TAG, "Inputs started: ENC_A=%d ENC_B=%d ENC_SW=%d NEXT=%d PLAY=%d PWR_OFF=%d",
             ENC_A, ENC_B, ENC_SW, BTN_NEXT, BTN_PLAY, PWR_OFF);

    xTaskCreate(inputs_task, "garage_inputs", 4096, NULL, 1, NULL);
}



