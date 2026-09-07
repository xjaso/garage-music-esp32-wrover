| Supported Targets | ESP32 |
| ----------------- | ----- |

AVRCP-CT-COVER-ART EXAMPLE
======================
# Garage Music ESP32 WROVER

## 🇸🇰 Slovensky

## Na čo to slúži

Garage Music ESP32 WROVER je nástenný Bluetooth ovládač hudby do garáže.

Zariadenie sa pripojí k telefónu cez Bluetooth a zobrazuje informácie o aktuálne prehrávanej skladbe:

- názov skladby
- interpret
- názov albumu
- čas prehrávania
- celkový čas skladby
- progress bar
- obal albumu, ak ho telefón poskytne

Zároveň funguje ako diaľkové ovládanie hudby:

- play / pause
- next track
- previous track
- volume up / down
- mute
- reconnect / pairing
- power off cez Pololu vypínač

Projekt používa ESP32-WROVER s PSRAM, ST7789 displej, LVGL UI, Bluetooth Classic AVRCP/A2DP metadata, HID media tlačidlá a vypínanie cez Pololu 2808.

---

## Dôležité upozornenie k zvuku

ESP32 v tomto projekte nie je určené ako audio výstup.

Bluetooth profil A2DP je použitý hlavne preto, aby telefón poskytoval AVRCP metadata a cover art. Audio stream do ESP32 je v softvéri vypnutý / nepoužíva sa ako výstup.

Ak sa po pripojení telefónu začne hudba prehrávať „do ESP32“ a nepočuješ ju z reproduktorov, treba v telefóne prepnúť audio výstup späť na skutočné výstupné zariadenie, napríklad:

- autorádio
- Bluetooth zosilňovač
- reproduktor
- slúchadlá
- AUX / USB audio zariadenie

ESP32 má v tomto projekte slúžiť ako displej a ovládač, nie ako reproduktor.

---

## Ako sa pripojiť

1. Zapni zariadenie POWER tlačidlom.
2. Na displeji sa zobrazí UI.
3. Otvor Bluetooth nastavenia v telefóne.
4. Vyhľadaj zariadenie ESP32 / Garage Music.
5. Spáruj a pripoj sa.
6. Pusť hudbu v prehrávači, napríklad Spotify, YouTube Music alebo lokálny prehrávač.
7. Ak zvuk nejde do správneho zariadenia, v telefóne otvor výber audio výstupu a zvoľ reproduktor / zosilňovač.

---

## Ako fungujú informácie o skladbe

Telefón posiela informácie o prehrávanej skladbe cez Bluetooth Classic profil AVRCP.

ESP32 prijíma AVRCP metadata:

- title
- artist
- album
- playback position
- track length
- cover art handle / image data

Obal albumu sa stiahne cez AVRCP Cover Art, dekóduje sa ako JPEG a zobrazí sa na ST7789 displeji cez LVGL.

Na fungovanie je potrebné, aby telefón a prehrávač podporovali AVRCP metadata. Niektoré aplikácie alebo telefóny nemusia posielať všetko. Vtedy môže chýbať napríklad album art alebo celkový čas skladby.

---

## Ako funguje ovládanie

Ovládanie je riešené cez Bluetooth HID Consumer Control.

ESP32 neposiela obyčajné AVRCP passthrough príkazy, ale správa sa ako HID media ovládač. Telefón ho teda vidí podobne ako Bluetooth klávesnicu / diaľkové ovládanie s multimediálnymi klávesmi.

Výhoda je, že tlačidlá vedia ovládať telefón aj vtedy, keď audio výstup hrá cez iné zariadenie.

### Funkcie tlačidiel

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

## Ako prepnúť zvuk na výstupné zariadenie

Po pripojení ESP32 môže telefón niekedy zvoliť ESP32 ako Bluetooth audio zariadenie. Vtedy hudba môže „hrať do ESP“, ale fyzicky ju nepočuješ, pretože tento projekt nemá audio výstup.

Riešenie:

### Android

1. Pusť hudbu.
2. Stiahni hornú lištu.
3. Klikni na výber výstupu zvuku / Media output.
4. Vyber skutočné audio zariadenie:
   - reproduktor
   - auto rádio
   - Bluetooth zosilňovač
   - slúchadlá
5. ESP32 nechaj pripojené kvôli metadátam a ovládaniu.

### iPhone

1. Pusť hudbu.
2. Otvor Control Center.
3. Klikni na AirPlay / výber audio výstupu.
4. Vyber skutočný reproduktor alebo zosilňovač.
5. ESP32 nechaj pripojené ako ovládač / metadata zariadenie.

---
# 🇬🇧 English

## What it is used for

Garage Music ESP32 WROVER is a wall-mounted Bluetooth music controller for a garage.

The device connects to a phone via Bluetooth and displays information about the currently playing track:

- track title
- artist
- album name
- playback time
- total track time
- progress bar
- album cover art, if provided by the phone

It also works as a remote control for music playback:

- play / pause
- next track
- previous track
- volume up / down
- mute
- reconnect / pairing
- power off through the Pololu power switch

The project uses ESP32-WROVER with PSRAM, ST7789 display, LVGL UI, Bluetooth Classic AVRCP/A2DP metadata, HID media buttons and power-off control through Pololu 2808.

---

## Important audio note

ESP32 in this project is not intended to be used as an audio output.

The A2DP Bluetooth profile is used mainly so the phone can provide AVRCP metadata and cover art. The audio stream to the ESP32 is disabled / not used as an output in the firmware.

If, after connecting the phone, the music starts playing “to the ESP32” and you cannot hear it from your speakers, you need to switch the audio output on the phone back to the real output device, for example:

- car radio
- Bluetooth amplifier
- speaker
- headphones
- AUX / USB audio device

In this project, the ESP32 is meant to work as a display and controller, not as a speaker.

---

## How to connect

1. Turn on the device using the POWER button.
2. The UI will appear on the display.
3. Open Bluetooth settings on your phone.
4. Search for the ESP32 / Garage Music device.
5. Pair and connect to it.
6. Start playing music in an app such as Spotify, YouTube Music or a local music player.
7. If the sound is not playing through the correct device, open the audio output selector on your phone and choose the speaker / amplifier.

---

## How track information works

The phone sends information about the currently playing track through the Bluetooth Classic AVRCP profile.

The ESP32 receives AVRCP metadata:

- title
- artist
- album
- playback position
- track length
- cover art handle / image data

The album cover is downloaded through AVRCP Cover Art, decoded as JPEG and shown on the ST7789 display using LVGL.

For this to work, the phone and the music player app must support AVRCP metadata. Some apps or phones may not send all information. In that case, album art or total track time may be missing.

---

## How the controls work

The controls are implemented using Bluetooth HID Consumer Control.

The ESP32 does not send normal AVRCP passthrough commands. Instead, it behaves like a HID media controller. The phone sees it similarly to a Bluetooth keyboard / remote control with multimedia keys.

The advantage is that the buttons can control the phone even when the audio output is playing through another device.

### Button functions

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

## How to switch sound to the output device

After connecting the ESP32, the phone may sometimes select the ESP32 as a Bluetooth audio device. In that case, the music may be “playing to the ESP”, but you will not hear it because this project has no audio output.

Solution:

### Android

1. Start playing music.
2. Pull down the notification bar.
3. Tap the audio output selector / Media output.
4. Select the real audio device:
   - speaker
   - car radio
   - Bluetooth amplifier
   - headphones
5. Keep the ESP32 connected for metadata and controls.

### iPhone

1. Start playing music.
2. Open Control Center.
3. Tap AirPlay / audio output selector.
4. Select the real speaker or amplifier.
5. Keep the ESP32 connected as a controller / metadata device.


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
