| Supported Targets | ESP32 |
| ----------------- | ----- |

AVRCP-CT-COVER-ART EXAMPLE
======================

This is an example demonstrating the API for implementing Audio/Video Remote Control Profile to get and display cover art image.

## Required components

- [bt_app_core_utils](../common/bt_app_core_utils)
- [bredr_app_common_utils](../common/bredr_app_common_utils)
- [a2dp_sink_common_utils](../common/a2dp_utils/a2dp_sink_common_utils)
- [a2dp_sink_int_codec_utils](../common/a2dp_utils/a2dp_sink_int_codec_utils)
- [a2dp_sink_ext_codec_utils](../common/a2dp_utils/a2dp_sink_ext_codec_utils)
- [avrcp_common_utils](../common/avrcp_utils/avrcp_common_utils)
- [avrcp_metadata_utils](../common/avrcp_utils/avrcp_metadata_utils)
- [avrcp_cover_art_utils](../common/avrcp_utils/avrcp_cover_art_utils)

```
+---------------------------------------------------+---------------------+
|                avrcp_cover_art_utils              |                     |
+---------------------------------------------------+                     |
|                avrcp_metadata_utils               |                     |
+---------------------------------------------------+                     |
|                 avrcp_common_utils                |                     |
+-------------------------+-------------------------+  bt_app_core_utils  |
|a2dp_sink_int_codec_utils|a2dp_sink_ext_codec_utils|                     |
+-------------------------+-------------------------+                     |
|               a2dp_sink_common_utils              |                     |
+---------------------------------------------------+                     |
|               bredr_app_common_utils              |                     |
+---------------------------------------------------+---------------------+
```

Detailed information can be viewed through the [../common/README.md](../common/README.md).

## How to use this example

### Hardware Required

* An ESP32 development board
* A SPI-interfaced LCD
* A USB cable for power supply and programming

### Hardware Connection

The connection between ESP32 Board and the LCD is as follows:

```
      ESP32 Board                          LCD Screen
      +---------+              +---------------------------------+
      |         |              |                                 |
      |     3V3 +--------------+ VCC   +----------------------+  |
      |         |              |       |                      |  |
      |     GND +--------------+ GND   |                      |  |
      |         |              |       |                      |  |
      |   DATA0 +--------------+ MOSI  |                      |  |
      |         |              |       |                      |  |
      |    PCLK +--------------+ SCK   |                      |  |
      |         |              |       |                      |  |
      |      CS +--------------+ CS    |                      |  |
      |         |              |       |                      |  |
      |     D/C +--------------+ D/C   |                      |  |
      |         |              |       |                      |  |
      |     RST +--------------+ RST   |                      |  |
      |         |              |       |                      |  |
      |BK_LIGHT +--------------+ BCKL  +----------------------+  |
      |         |              |                                 |
      +---------+              +---------------------------------+
```

The GPIO number used by this example can be changed in [avrcp_cover_art_service.c](../common/avrcp_utils/avrcp_cover_art_utils/avrcp_cover_art_service.c), where:

| GPIO number              | LCD pin |
| ------------------------ | ------- |
| EXAMPLE_PIN_NUM_PCLK     | SCK     |
| EXAMPLE_PIN_NUM_CS       | CS      |
| EXAMPLE_PIN_NUM_DC       | DC      |
| EXAMPLE_PIN_NUM_RST      | RST     |
| EXAMPLE_PIN_NUM_DATA0    | MOSI    |
| EXAMPLE_PIN_NUM_BK_LIGHT | BCKL    |

Note that the level used to turn on the LCD backlight may vary: some LCD modules need a low level to turn it on, while others require a high level. You can change the backlight level macro `EXAMPLE_LCD_BK_LIGHT_ON_LEVEL` in [avrcp_cover_art_service.c](../common/avrcp_utils/avrcp_cover_art_utils/avrcp_cover_art_service.c).

### Configure the project

```
idf.py menuconfig
```

* The AVRCP CT Cover Art feature is enabled by default. We can disable it by unselecting the menuconfig option `Component config --> Bluetooth --> Bluedroid Options --> Classic Bluetooth --> AVRCP Features --> AVRCP CT Cover Art`. This example will try to use the AVRCP CT Cover Art feature to get the cover art image, count the image size, and display it if the peer device supports it.
* **Memory Configuration**: To ensure that the A2DP sink stream and the display of AVRCP cover art on the LCD can run simultaneously, the following configurations are required and have been set in `sdkconfig.defaults`:
  * `CONFIG_SPIRAM=y`: Enables external SPI RAM (PSRAM) support. This provides additional memory space needed for buffering audio data during A2DP streaming while simultaneously handling cover art image decoding and display operations. Without this, the application may run out of internal RAM when processing both audio streams and image data.
  * `CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y`: Selects the single large app partition table scheme, which allocates more space for the application binary. This is necessary because the example application includes multiple Bluetooth profiles (A2DP, AVRCP), image decoding libraries, and LCD display drivers, requiring a larger application partition than the default partition table provides.

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output.

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

## Example Output

The output when receiving a cover art image:

```
I (31579) RC_CT: AVRC metadata rsp: attribute id 0x80, 1000526
I (32039) RC_CA_SRV: Cover Art Client final data event, image size: 12315 bytes
I (32119) RC_CA_SRV: JPEG image decoded! Size of the decoded image is: 200px x 200px.
```

And then, the LCD screen will display the cover art image.

## Troubleshooting

For any technical queries, please open an [issue](https://github.com/espressif/esp-idf/issues) on GitHub. We will get back to you soon.
# Garage Music ESP32 WROVER

## 🇸🇰 Slovensky

Bluetooth Classic ovládač hudby do garáže.

Projekt používa ESP32-WROVER s PSRAM, ST7789 displej, LVGL UI, AVRCP metadata, album art, HID media tlačidlá a vypínanie napájania cez Pololu power switch.

---

## Hardware

- ESP32-WROVER
- ST7789 320x240 SPI display
- Rotary encoder
- NEXT / PREV tlačidlá
- Pololu 2808 Mini Pushbutton Power Switch LV
- Mean Well IRM-10-5 5V zdroj

---

## Pinout

### ST7789 display

| Funkcia | GPIO |
|---|---|
| MOSI | GPIO23 |
| SCK | GPIO18 |
| CS | GPIO5 |
| DC | GPIO21 |
| RST | GPIO22 |
| BL | GPIO13 |

### Ovládanie

| Funkcia | GPIO |
|---|---|
| Encoder A | GPIO32 |
| Encoder B | GPIO33 |
| Encoder SW | GPIO14 |
| NEXT | GPIO19 |
| PREV / PLAY | GPIO25 |
| Pololu OFF | GPIO27 |

---

## Funkcie tlačidiel

| Akcia | Funkcia |
|---|---|
| Encoder doprava | Volume Up |
| Encoder doľava | Volume Down |
| Encoder click | Play / Pause |
| Encoder hold | Mute |
| NEXT click | Next Track |
| NEXT hold | Reconnect / Pairing |
| PREV click | Previous Track |
| PREV hold | Power off cez Pololu GPIO27 |

---

## Zapojenie Pololu 2808

```text
5V zdroj +  -> Pololu VIN
5V zdroj -  -> Pololu GND

Pololu VOUT -> ESP32 5V/VIN
Pololu GND  -> ESP32 GND

Power tlačidlo -> Pololu A-B
ESP GPIO27     -> Pololu OFF
```

Piny `ON` a `CTRL` sa nepoužívajú.

---

## Ako nahrať firmware do ESP32

Otvor **ESP-IDF PowerShell**.

### 1. Stiahni projekt

```powershell
cd C:\ESP32
git clone https://github.com/xjaso/garage-music-esp32-wrover.git
cd garage-music-esp32-wrover
```

Ak už projekt máš:

```powershell
cd C:\ESP32\garage-music-esp32-wrover
git pull
```

### 2. Zisti COM port

```powershell
Get-CimInstance Win32_SerialPort | Select-Object DeviceID,Name
```

Príklad:

```text
COM9  USB-SERIAL CH9102
```

### 3. Vyčisti build

```powershell
Remove-Item -Recurse -Force .\build -ErrorAction SilentlyContinue
```

### 4. Nastav target

```powershell
idf.py set-target esp32
```

### 5. Build

```powershell
idf.py build
```

### 6. Flash do ESP32

```powershell
idf.py -p COM9 -b 115200 flash
```

Ak máš iný port, zmeň `COM9`.

### 7. Monitor

```powershell
idf.py -p COM9 -b 115200 monitor
```

Ukončenie monitora:

```text
Ctrl + ]
```

---

## Čo má byť v logu

Po štarte má byť vidieť napríklad:

```text
SPI Flash Size : 8MB
Found 4MB PSRAM device
PSRAM initialized
GARAGE_INPUT: Inputs started
LVGL 8 UI started
Bluetooth init
HIDD register app OK
```

---

## Poznámky

Nepoužívať GPIO16/GPIO17 na ESP32-WROVER, lebo súvisia s PSRAM.

Pre tento firmware je potrebný **Bluetooth Classic**. ESP32-S3, ESP32-C3, ESP32-C6 a ESP32-S2 nie sú vhodné pre tento projekt.

---

---

# 🇬🇧 English

Bluetooth Classic garage music controller.

This project uses ESP32-WROVER with PSRAM, ST7789 display, LVGL UI, AVRCP metadata, album cover art, HID media keys and power-off control through a Pololu power switch.

---

## Hardware

- ESP32-WROVER
- ST7789 320x240 SPI display
- Rotary encoder
- NEXT / PREV buttons
- Pololu 2808 Mini Pushbutton Power Switch LV
- Mean Well IRM-10-5 5V power supply

---

## Pinout

### ST7789 display

| Function | GPIO |
|---|---|
| MOSI | GPIO23 |
| SCK | GPIO18 |
| CS | GPIO5 |
| DC | GPIO21 |
| RST | GPIO22 |
| BL | GPIO13 |

### Controls

| Function | GPIO |
|---|---|
| Encoder A | GPIO32 |
| Encoder B | GPIO33 |
| Encoder SW | GPIO14 |
| NEXT | GPIO19 |
| PREV / PLAY | GPIO25 |
| Pololu OFF | GPIO27 |

---

## Button functions

| Action | Function |
|---|---|
| Encoder clockwise | Volume Up |
| Encoder counter-clockwise | Volume Down |
| Encoder click | Play / Pause |
| Encoder hold | Mute |
| NEXT click | Next Track |
| NEXT hold | Reconnect / Pairing |
| PREV click | Previous Track |
| PREV hold | Power off through Pololu GPIO27 |

---

## Pololu 2808 wiring

```text
5V power supply +  -> Pololu VIN
5V power supply -  -> Pololu GND

Pololu VOUT        -> ESP32 5V/VIN
Pololu GND         -> ESP32 GND

Power button       -> Pololu A-B
ESP GPIO27         -> Pololu OFF
```

Pins `ON` and `CTRL` are not used.

---

## How to flash firmware to ESP32

Open **ESP-IDF PowerShell**.

### 1. Clone the project

```powershell
cd C:\ESP32
git clone https://github.com/xjaso/garage-music-esp32-wrover.git
cd garage-music-esp32-wrover
```

If the project is already cloned:

```powershell
cd C:\ESP32\garage-music-esp32-wrover
git pull
```

### 2. Check the COM port

```powershell
Get-CimInstance Win32_SerialPort | Select-Object DeviceID,Name
```

Example:

```text
COM9  USB-SERIAL CH9102
```

### 3. Clean previous build

```powershell
Remove-Item -Recurse -Force .\build -ErrorAction SilentlyContinue
```

### 4. Set target

```powershell
idf.py set-target esp32
```

### 5. Build

```powershell
idf.py build
```

### 6. Flash to ESP32

```powershell
idf.py -p COM9 -b 115200 flash
```

Change `COM9` if your board uses another port.

### 7. Monitor

```powershell
idf.py -p COM9 -b 115200 monitor
```

Exit monitor:

```text
Ctrl + ]
```

---

## Expected boot log

After boot, the log should contain something like:

```text
SPI Flash Size : 8MB
Found 4MB PSRAM device
PSRAM initialized
GARAGE_INPUT: Inputs started
LVGL 8 UI started
Bluetooth init
HIDD register app OK
```

---

## Notes

Do not use GPIO16/GPIO17 on ESP32-WROVER because they are related to PSRAM.

This firmware requires **Bluetooth Classic**. ESP32-S3, ESP32-C3, ESP32-C6 and ESP32-S2 are not suitable for this project.
