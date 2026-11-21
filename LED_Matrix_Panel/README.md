# LED Matrix Panel

*English · [Česky](../README.cs.md)*

Clock, nearby aircraft and ČHMÚ precipitation radar on a
**Waveshare RGB LED matrix 96×48, P2.5 (HUB75)** driven by the
**LaskaKit ESP32-S3-DEVKit**. Everything is configured from a web page
and stored in the ESP32's flash.

## What it shows

**Screens:** clock (analog with anti-aliased hands or digital), forecast (now · today ·
tomorrow), aircraft (rotated icon, callsign, type, speed, altitude, distance, "+N" for more
aircraft in range), radar (animated map with a small clock).

**Screen modes** (chosen on the web, remembered over a restart):

| Mode | Behaviour |
|------|-----------|
| Automatic | clock, every 50 s the forecast for 10 s (adjustable); rain within the radius → radar instead; aircraft *at once* (has priority), *now and then* (e.g. 15 s every 60 s while one is around) or *never* |
| Rotation | the chosen screens in turn, N seconds each; aircraft skipped when none is around, radar skipped without rain nearby (unless "radar also without rain") |
| Fixed | clock, forecast, aircraft or radar only |

Aircraft: always the nearest one; with a viewing direction set, an aircraft in that sector
(direction ± width, own range) is shown even beyond the radius and has priority.
Night (optional): dimmed, clock only, or display off (off applies in every mode).

On the radar map, cities are blue 5-LED crosses (Praha, Plzeň, Liberec, Ústí n. L., Hradec Králové,
Jihlava, České Budějovice, Brno, Olomouc, Ostrava). Your position is a green LED; when it falls
on a city (or one pixel next to it), the cross has a green centre and blue arms.

Radar source (web, Radar tab):

| Source | What it is |
|--------|------------|
| ČHMÚ | MAX Z composite (`png`), every echo the radars see, new frame every 5 min |
| ČHMÚ masked | the same composite from `png_masked`: only precipitation that reaches the ground (no virga, no anvils of distant storms) |
| RainViewer | international composite, Web Mercator tiles (zoom 6, 2×2 tiles of 256 px), new frame every 10 min, coarser |

All sources end up in the same 4-bit grid with the ČHMÚ levels (4–60 dBZ), so the map, the legend
and the rain threshold work the same. A frame already shown is not downloaded again.

Language: Czech or English for the panel texts and the web page (Systém → Jazyk / Language).

## Ready-made firmware

No Arduino IDE needed: flash a file from the `bin` folder from offset **0x0**,
e.g. in Chrome / Edge with [esp32flasher.chiptron.cz](https://esp32flasher.chiptron.cz).

| File | Use |
|------|-----|
| `LED_Matrix_Panel_EN.merged.bin` | first installation, panel and web in **English** |
| `LED_Matrix_Panel_CZ.merged.bin` | first installation, panel and web in **Czech** |
| `LED_Matrix_Panel.bin` | later updates over WiFi (*Systém → Aktualizace firmwaru*), keeps all settings |

The language of the file only sets the default. A board that already has settings stored
keeps its language; erase the flash before flashing (or use *Factory reset*) to get the
default, or simply switch it on the web page (Systém → Jazyk / Language).

## First start

1. The panel opens an open WiFi network **`LED-Matrix-XXXX`** and shows a QR code to join it.
2. Join it with a phone – the settings page opens by itself (or go to `http://192.168.4.1`).
3. On the **WiFi** tab choose your home network and enter the password.
4. The set-up network closes, the panel connects to the home WiFi and shows its
   address for 10 s (on every start), e.g. `192.168.1.39` and `ledmatrix.local`.
   Then the clock appears. Your phone falls back to its usual network.

If the home WiFi cannot be reached within 20 s (wrong password, router off), the set-up
network comes back. With a stored network it closes every 5 minutes for one more try,
but only while no phone is connected to it.

The radio is always either a station or an access point, never both – in the mixed
AP+STA mode the station searches other channels and phones on the set-up network
keep losing the connection.

## Web interface (Czech / English)

| Tab | Settings |
|-----|----------|
| Stav | time, current screen, aircraft, radar, weather, memory; screen mode (auto / rotation / clock / aircraft / radar / forecast) |
| WiFi | network scan, connect, forget |
| Poloha | search a town (Open-Meteo geocoding, runs in the browser), latitude / longitude |
| Letadla | on/off, radius, minimum altitude, callsign only, poll interval, minimum show time; viewing direction (8 directions), ± width, own range, with a small map of the sector |
| Radar | on/off, source (ČHMÚ / ČHMÚ masked / RainViewer), rain radius, dBZ threshold, frames, animation speed, hold, legend, cities, map background |
| Hodiny a počasí | analog / digital, seconds, date; colours of hands, seconds, digits, dial marks, weekday + month, day + year; forecast on/off and timing |
| Displej | automatic mode (aircraft at once / now and then / never), rotation (screens, interval, radar without rain); brightness day / night, night by time or sunset–sunrise, dim or off, clock only |
| Systém | language, host name, time zone, backup / restore (JSON), firmware update, restart, factory reset |

Settings are kept in NVS (Preferences) – they survive firmware updates. The `DEFAULT_*`
values in `config.h` are used only on the very first start or after a factory reset.

JSON API (used by the page, handy for scripts):
`GET /api/status`, `GET|POST /api/config`, `GET /api/scan`, `POST /api/wifi`,
`POST /api/wifi/forget`, `POST /api/mode`, `GET /api/export`, `POST /api/import`,
`POST /api/reboot`, `POST /api/reset`, `POST /update` (firmware .bin).

## Hardware

| Part | Note |
|------|------|
| LaskaKit ESP32-S3-DEVKit | with or without PSRAM |
| Waveshare RGB-Matrix-P2.5-96x48-F | HUB75, 1/24 scan, V2 = SM5368 row driver |
| 5 V power supply, 4 A or more | the panel is **not** powered from the ESP32 |

## Wiring (14 GPIO)

On the 96x48-F **V2** panel rows are driven by an SM5368 shift register through A, B, C only;
D and E are not used by the panel (wiring them does no harm).

| HUB75 pin | Signal | ESP32-S3 GPIO | DEVKit header |
|-----------|--------|---------------|---------------|
| 1  | R1  | 4  | left  |
| 2  | G1  | 5  | left  |
| 3  | B1  | 6  | left  |
| 5  | R2  | 7  | left  |
| 6  | G2  | 15 | left  |
| 7  | B2  | 16 | left  |
| 9  | A   | 17 | left  |
| 10 | B   | 18 | left  |
| 11 | C   | 8  | left  |
| 12 | D   | 41 | right |
| 8  | E   | 21 | right |
| 14 | LAT | 40 | right |
| 15 | OE  | 39 | right |
| 13 | CLK | 38 | right |
| 4, 16 | GND | GND | both |

**Always connect GND of the ESP32 to GND of the panel power supply.**
Pins can be changed in `config.h`.

Avoided on purpose: 0, 3, 45, 46 (strapping), 45 (RGB LED), 19/20 (USB),
43/44 (UART), 2/42 (µŠup I²C), 47 (µŠup power), 9 (battery ADC), 35–37 (octal PSRAM).

## Software

1. Arduino IDE 2.x + **esp32 by Espressif Systems** board package (3.x)
2. Library Manager:
   - **ESP32 HUB75 LED MATRIX PANEL DMA Display** (by mrcodetastic, **v3.1.0 or newer** – SM5368 support),
     Adafruit GFX is installed as a dependency
   - **PNGdec** (by Larry Bank) – radar images
   - **ArduinoJson** (by Benoit Blanchon, v7) – aircraft data, web API
3. Board **ESP32S3 Dev Module**, *Tools*:
   - **PSRAM** – match the module: *OPI* (…R8), *QSPI* (…R2), *Disabled* when there is none
   - **Flash Size: 16MB (128Mb)**
   - **Partition Scheme: 16M Flash (3MB APP/9.9MB FATFS)** – two app slots, so the web update works
4. No `build_opt.h` – the libraries run with their default settings. If an older version
   of this project left a `build_opt.h` in the sketch folder, delete it **and** the build cache
   (folder `arduino/sketches` in the system temp folder, Windows `%LOCALAPPDATA%\Temp`).
5. Upload through the USB-C connector labelled **UART**
   (for the Serial Monitor on that port keep *USB CDC On Boot: Disabled*)

Later updates can go through the web page: *Sketch → Export Compiled Binary* in Arduino IDE,
then *Systém → Aktualizace firmwaru*.

## Project structure

```
LED_Matrix_Panel.ino  setup() / loop() only
config.h          pins, panel type, defaults, limits, DEBUG switches
settings.h/.cpp   user settings in flash (NVS) + JSON
wifi_setup.h/.cpp home WiFi, set-up access point, captive portal, mDNS
web.h/.cpp        web server, JSON API, firmware update
web_page.h        the settings page (HTML/CSS/JS in one string)

station.h/.cpp    which screen to show, night mode, brightness
screens.h/.cpp    clock, aircraft, radar, set-up and IP screens
sky_data.h/.cpp   data shared by network task and display (mutex)
fetcher.h/.cpp    background network task on core 0
net.h/.cpp        HTTPS helpers (streaming, growing buffers)
time_sync.h/.cpp  clock from the HTTP "Date" header
sun.h/.cpp        sunrise / sunset (NOAA)
adsb.h/.cpp       nearest aircraft from adsb.fi + count in range
chmu.h/.cpp       ČHMÚ radar (plain / masked): directory index, PNG download, decoding
rainviewer.h/.cpp RainViewer radar: index JSON, tiles, colour -> dBZ
png_stream.h      feeds PNGdec straight from the HTTPS stream (sliding window)
forecast.h/.cpp   Open-Meteo: weather now, today, tomorrow
weather_icons.h/.cpp  16x16 weather icons drawn with GFX primitives
radar_palette.h   ČHMÚ colour scale, 4-bit radar grid (2304 B / frame)
radar_map.h/.cpp  Czech map 96x48 (Natural Earth 1:10m, Web Mercator) + projection
map_view.h/.cpp   map layers: land, border, cities, home, legend, rain
qrcode.h/.c       QR code generator (ricmoo/QRCode, MIT)

display.h/.cpp    panel init, drawing canvas, changed-pixel transfer, brightness
colors.h          RGB565 helpers
debug.h           DEBUG_LOG() macros
console.h/.cpp    serial test commands
```

How it works:
- **Main loop** (core 1): WiFi state machine, web server, screen choice and drawing. A screen
  is redrawn only when its content changes (clock tick, radar frame, new data); it is drawn
  into a canvas in RAM and only the pixels that differ are written to the panel's DMA buffer.
- **Network task** (core 0): asks adsb.fi every few seconds, ČHMÚ every 5 minutes and Open-Meteo every 15 minutes,
  publishes results into `SkyData`. Downloads never block the display or the web page.
- **Radar**: the directory listing is scanned as a stream for the newest file names,
  each PNG is decoded line by line *while it downloads* (a 4 kB sliding window,
  no buffer for the whole file) straight into a 96×48 grid; every
  panel pixel keeps the strongest echo of the area it covers. The newest frame is
  also checked for rain within the radius.
- **Time** comes from the `Date` header of those responses (no NTP). With aircraft,
  radar and forecast all off, a tiny request is made every 30 minutes just for the time.

## Debug

```cpp
#define DEBUG_ENABLED  1   // 0 = no serial output, no debug code in firmware
#define DEBUG_CONSOLE  1   // single-key commands in the Serial Monitor
```

Console (115200 baud): `m` screen mode AUTO/CLOCK/PLANE/RADAR, `r` download radar now,
`+`/`-` brightness, `i` info (time, WiFi, aircraft, radar, memory), `h` help.

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| Image shifted by 1 px, text unreadable | toggle `PANEL_CLK_PHASE` (Waveshare = `false`) |
| The start-up bars R, G, B show other colours | `PANEL_SWAP_RB` in `config.h` swaps red and blue (Waveshare 96x48-F V2 = `1`) |
| Wrong colours (white clock hands look blue, dark colours missing) | a stale `build_opt.h` from an older version – delete it and the build cache, see *Software* step 4 |
| Whole rows lit / stripes | `PANEL_LINE_DECODER` (96x48-F V2 = `SM5368`, V1 = `TYPE138`) |
| Boot log "PSRAM chip is not connected" | set *Tools → PSRAM* to match the module (or Disabled) |
| Neighbouring LEDs glow / flicker | check the panel GND, a short ribbon cable and the 5 V supply; `PANEL_I2S_SPEED HZ_8M` in `config.h` |
| `HTTP -1 connection refused` for every request | usually too little RAM for TLS – the log line shows the largest free block (TLS needs two ~17 kB blocks); fewer radar frames |
| Web page not reachable | check the address shown after start; `http://ledmatrix.local` needs mDNS (works on most phones and PCs) |
| ESP32 resets at high brightness | stronger 5 V supply, lower brightness in the web interface |

## Data and licences

- Aircraft: [adsb.fi](https://adsb.fi) open data (non-commercial use)
- Radar: [ČHMÚ open data](https://opendata.chmi.cz) (MAX Z composite, `png` / `png_masked`)
- Radar: [RainViewer](https://www.rainviewer.com/api.html) (personal / non-commercial use, max. 100 requests/min)
- Forecast: [Open-Meteo](https://open-meteo.com) (CC BY 4.0, free for non-commercial use)
- Map: [Natural Earth](https://www.naturalearthdata.com) (public domain)
- Town search: [Open-Meteo geocoding](https://open-meteo.com) (in the browser)
- QR code: ricmoo/QRCode (MIT)
