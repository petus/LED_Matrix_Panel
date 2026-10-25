/**
 * @file    LED_Matrix_Panel.ino
 * @brief   LED Matrix Panel – clock, nearby aircraft and ČHMÚ precipitation
 *          radar on a HUB75 RGB LED matrix, configured from a web page.
 *
 * Hardware:
 *   - LaskaKit ESP32-S3-DEVKit
 *   - Waveshare RGB LED matrix 96x48 flexible, pitch 2.5 mm (HUB75, 1/24 scan)
 *   - 5 V / 4 A (or stronger) power supply for the panel
 *
 * Arduino IDE board settings (ESP32S3 Dev Module):
 *   PSRAM: as on the module (OPI / QSPI / Disabled), Flash Size: 16MB,
 *   Partition: 16M Flash (3MB APP/9.9MB FATFS)  – two app slots, so OTA works
 *
 * Libraries (Arduino Library Manager):
 *   - "ESP32 HUB75 LED MATRIX PANEL DMA Display" by mrcodetastic, v3.1.0+
 *   - "Adafruit GFX Library" (installed automatically as a dependency)
 *   - "PNGdec" by Larry Bank            – ČHMÚ radar images
 *   - "ArduinoJson" by Benoit Blanchon  – adsb.fi answers, web API (v7)
 *   WebServer, DNSServer, ESPmDNS, Preferences and Update come with the ESP32 core.
 *
 * First start:
 *   The panel opens the WiFi network "LED-Matrix-XXXX" and shows a QR code.
 *   Join it with a phone, the settings page opens (or go to 192.168.4.1),
 *   choose the home WiFi. After connecting, the panel shows its address for
 *   10 s – the same page is then at http://<address>/ or http://ledmatrix.local/.
 *
 * Data sources:
 *   - aircraft: adsb.fi open data API
 *   - radar:    opendata.chmi.cz, MAX Z composite PNG
 *   - forecast: api.open-meteo.com
 *   - time:     "Date" header of those HTTP responses
 *   Inspired by MeteoPlaneRadar (github.com/petus/MeteoPlaneRadar).
 *
 * Project structure:
 *   LED_Matrix_Panel.ino  setup() and loop() – keeps only the program flow
 *   config.h          pins, panel type, defaults, limits, debug switches
 *   settings.h/.cpp   user settings in flash (NVS) + JSON for the web
 *   wifi_setup.h/.cpp home WiFi, set-up access point, captive portal, mDNS
 *   web.h/.cpp        web server, JSON API, firmware update
 *   web_page.h        the settings page (HTML/CSS/JS, Czech)
 *
 *   station.h/.cpp    which screen to show, night mode, brightness
 *   screens.h/.cpp    clock, aircraft, radar, set-up and IP screens
 *   sky_data.h/.cpp   data shared between network task and display (mutex)
 *   fetcher.h/.cpp    background network task (core 0)
 *   net.h/.cpp        HTTPS helpers
 *   time_sync.h/.cpp  clock from the HTTP "Date" header
 *   sun.h/.cpp        sunrise / sunset for the night mode
 *   adsb.h/.cpp       nearest aircraft from adsb.fi + count in range
 *   chmu.h/.cpp       ČHMÚ radar download and decoding
 *   png_stream.h      PNG decoding straight from the network stream
 *   forecast.h/.cpp   weather now / today / tomorrow from Open-Meteo
 *   weather_icons.h/.cpp  16x16 weather icons
 *   radar_palette.h   ČHMÚ colour scale, compact radar grid
 *   radar_map.h/.cpp  Czech Republic map 96x48 + projection
 *   map_view.h/.cpp   map layers: land, border, cities, home, legend
 *   qrcode.h/.c       QR code generator (ricmoo/QRCode, MIT)
 *
 *   display.h/.cpp    panel initialisation, brightness, frame buffer
 *   colors.h          RGB565 colours and helpers
 *   debug.h           DEBUG_LOG() macros for the Serial Monitor
 *   console.h/.cpp    serial test commands (debug only)
 *
 * Author: chiptron.cz
 */
#include "config.h"
#include "debug.h"
#include "display.h"
#include "settings.h"
#include "wifi_setup.h"
#include "web.h"
#include "station.h"
#include "console.h"

#include <esp_system.h>

// Why did the chip (re)start? Helps to spot power problems (brownout).
const char* resetReason() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:  return "power on";
    case ESP_RST_SW:       return "software restart";
    case ESP_RST_PANIC:    return "crash (panic)";
    case ESP_RST_BROWNOUT: return "BROWNOUT – supply voltage dropped";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:      return "watchdog";
    default:               return "other";
  }
}

void setup() {
  DEBUG_BEGIN();
  DEBUG_LOG("MAIN", "=== %s %s ===", FW_NAME, FW_VERSION);
  DEBUG_LOG("MAIN", "Reset reason: %s", resetReason());

  if (!Display::begin()) {
    DEBUG_LOG("MAIN", "Display init failed, halting");
    while (true) delay(1000);
  }
  Display::colourTest();   // R, G, B bars – check the colours and PANEL_SWAP_RB
  delay(1500);

  Settings::begin();
  Station::begin();        // big buffers first, while the heap is in one piece
  WifiSetup::begin();
  Web::begin();
  Console::begin();
}

void loop() {
  WifiSetup::loop();
  Web::loop();
  Console::update();
  Station::update();
  delay(1);                // let the idle task and WiFi breathe
}
