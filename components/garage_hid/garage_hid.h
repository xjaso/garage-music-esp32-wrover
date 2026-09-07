#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GARAGE_HID_VOL_UP = 0,
    GARAGE_HID_VOL_DOWN,
    GARAGE_HID_PLAY_PAUSE,
    GARAGE_HID_NEXT,
    GARAGE_HID_PREV,
    GARAGE_HID_MUTE,
} garage_hid_cmd_t;

void garage_hid_start(void);
void garage_hid_send(garage_hid_cmd_t cmd);
void garage_hid_reconnect(void);

#ifdef __cplusplus
}
#endif



